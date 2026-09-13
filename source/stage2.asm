;
; Copyright 2026 Aarav Ravindra Kharade
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.
;


[BITS 16]
[ORG 0x7E00]

    ; Save boot drive passed in dl by the bootloader
    mov [boot_drive], dl

    ; Set VESA Video Mode (1024x768x32 - Bochs VBE mode)
    mov ax, 0x4F02
    mov bx, 0x4144       ; mode 0x144 + LFB bit = 32bpp guaranteed
    int 0x10

    ; Get VBE Mode Info to find the LFB address and pitch
    mov ax, 0x4F01
    mov cx, 0x0144
    mov di, 0x3000
    int 0x10
    movzx eax, word [0x3010] ; BytesPerScanLine (actual pitch)
    mov [screen_pitch], eax
    mov edi, [0x3028]     ; LFB Physical Address
    mov [lfb_addr], edi

    ; Enter Unreal Mode
    cli
    push ds
    push es

    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_desc]

    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp $+2

    mov bx, 0x08
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    mov ss, bx

    and al, 0xFE
    mov cr0, eax
    jmp $+2

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    sti

    ; Clear linear framebuffer (black screen) to prevent text bleed-through
    mov edi, [lfb_addr]
    xor eax, eax
    mov ecx, [screen_pitch]
    shr ecx, 2              ; dwords per scanline
    imul ecx, 768           ; total dwords for 768 lines
    a32 rep stosd



    ; Load animation.bin to 0x2000000 (32MB)
    mov eax, [animation_lba]
    mov ecx, [animation_size_sectors]
    test ecx, ecx
    jz .skip_animation
    mov edi, 0x2000000
    call read_sectors_high

    ; Play Animation (nucleus expansion only — electrons done by Swift later)
    mov ecx, 60
    mov esi, 0x2000000 ; Source of animation frames
.play_anim:
    push ecx
    mov edi, [lfb_addr]
    ; Center X = (1024-200)/2 = 412
    ; Center Y = (768-200)/2 = 284
    ; Dest offset = 284 * pitch + 412 * 4
    mov eax, 284
    imul eax, dword [screen_pitch]
    add eax, 412 * 4
    add edi, eax

    mov cx, 200 ; 200 lines
.draw_line:
    push ecx
    push edi
    mov ecx, 200 ; 200 pixels * 4 bytes (32bpp) = 800 bytes = 200 dwords
    a32 rep movsd
    pop edi
    add edi, [screen_pitch] ; Advance by actual pitch
    pop ecx
    dec cx
    jnz .draw_line
    
    ; Wait ~16ms (16666 microseconds)
    mov ah, 0x86
    mov cx, 0 ; High word of microseconds
    mov dx, 16666 ; Low word of microseconds
    int 0x15
    
    pop ecx
    dec ecx
    jnz .play_anim

.skip_animation:
    ; 1. Read first sector of bzImage to get setup_sects
    mov eax, [bzimage_lba]
    mov ebx, 0x9000
    mov es, bx
    xor bx, bx
    mov cx, 1
    call read_sectors
    jc .err_read1

    mov al, [es:0x01F1]
    cmp al, 0
    jne .has_setup
    mov al, 4
.has_setup:
    inc al
    movzx cx, al

    ; 2. Read setup sectors to 0x90000
    mov eax, [bzimage_lba]
    mov bx, 0x9000
    mov es, bx
    xor bx, bx
    call read_sectors
    jc .err_read2

    ; 3. Read protected mode kernel to 0x100000
    mov eax, [bzimage_lba]
    add eax, ecx
    mov edx, [bzimage_size_sectors]
    sub edx, ecx
    mov ecx, edx
    mov edi, 0x100000
    call read_sectors_high
    jc .err_read3

    ; 4. Read initramfs to 0x8000000
    mov eax, [initramfs_lba]
    mov ecx, [initramfs_size_sectors]
    mov edi, 0x8000000
    call read_sectors_high
    jc .err_read4

    ; Setup Boot Params at 0x90000
    mov bx, 0x9000
    mov ds, bx

    ; Tell the Linux 16-bit setup code to use VESA mode 0x144 (0x344 = 0x144 + 0x200)
    ; This makes the kernel automatically probe the VESA mode and set up vesafb!
    mov word [0x1FA], 0x0344

    mov byte [0x210], 0xFF
    mov al, [0x211]
    or al, 0x80
    mov [0x211], al
    mov word [0x224], 0xFE00
    mov dword [0x218], 0x8000000
    
    push ds
    xor ax, ax
    mov ds, ax
    mov eax, [initramfs_size_bytes]
    pop ds
    mov dword [0x21C], eax
    
    mov dword [0x228], 0x99000

    push ds
    xor ax, ax
    mov ds, ax
    mov si, cmd_line
    pop es
    mov di, 0x9000
.copy_cmd:
    lodsb
    stosb
    test al, al
    jnz .copy_cmd

    mov ax, 0x9000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    cli
    jmp 0x9000:0x0200

.err_read1:
    mov si, msg_err1
    call print_string
    jmp halt
.err_read2:
    mov si, msg_err2
    call print_string
    jmp halt
.err_read3:
    mov si, msg_err3
    call print_string
    jmp halt
.err_read4:
    mov si, msg_err4
    call print_string
halt:
    hlt
    jmp halt

; --- Functions ---
print_string:
    push ds
    push ax
    xor ax, ax
    mov ds, ax
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    pop ax
    pop ds
    ret

; read_sectors: LBA in EAX, Count in CX, Buffer in ES:BX. Returns CF=1 on error.
read_sectors:
    pusha
.loop:
    push eax
    push cx
    
    mov [dap_lba], eax
    mov [dap_buf_offset], bx
    mov ax, es
    mov [dap_buf_segment], ax
    mov word [dap_count], 1

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .disk_error

    pop cx
    pop eax
    inc eax
    add bx, 512
    dec cx
    jnz .loop
    popa
    clc
    ret
.disk_error:
    pop cx
    pop eax
    popa
    stc
    ret

; read_sectors_high: LBA in EAX, Count in ECX, Dest Physical in EDI. Returns CF=1 on error.
read_sectors_high:
    pusha
.loop_high:
    push eax
    push ecx

    ; Use temporary buffer at 0x0000:0x2000
    mov word [dap_lba], ax
    shr eax, 16
    mov word [dap_lba+2], ax
    mov word [dap_buf_offset], 0x2000
    mov word [dap_buf_segment], 0x0000
    mov word [dap_count], 1

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    
    push edi    ; Preserve EDI against BIOS corruption
    int 0x13
    pop edi

    jc .disk_error_high

    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov esi, 0x2000
    mov ecx, 128
    a32 rep movsd
    pop es
    pop ds

    pop ecx
    pop eax
    inc eax
    dec ecx
    jnz .loop_high
    popa
    clc
    ret
.disk_error_high:
    pop ecx
    pop eax
    popa
    stc
    ret

; --- Data ---
msg_stage2: db "Stage 2 Bootloader...", 13, 10, 0
msg_unreal: db "Unreal Mode Activated...", 13, 10, 0
msg_setup_ok: db "Setup loaded.", 13, 10, 0
msg_kernel_ok: db "Kernel loaded.", 13, 10, 0
msg_initrd_ok: db "Initrd loaded.", 13, 10, 0
msg_err1: db "Error: Failed to read first kernel sector", 13, 10, 0
msg_err2: db "Error: Failed to read kernel setup sectors", 13, 10, 0
msg_err3: db "Error: Failed to read protected kernel", 13, 10, 0
msg_err4: db "Error: Failed to read initramfs", 13, 10, 0
boot_drive: db 0x80

align 16
global_vars:
bzimage_lba:          dd 8
bzimage_size_sectors: dd 0
initramfs_lba:        dd 0
initramfs_size_sectors: dd 0
initramfs_size_bytes: dd 0
animation_lba:        dd 0
animation_size_sectors: dd 0
lfb_addr:             dd 0
screen_pitch:         dd 0
cmd_line: db "console=ttyS0 init=/init root=/dev/sdb rw vt.global_cursor_default=0", 0

align 4
dap:
dap_size: db 0x10
dap_zero: db 0
dap_count: dw 1
dap_buf_offset: dw 0
dap_buf_segment: dw 0
dap_lba: dq 0

gdt_start:
    dq 0
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_desc:
    dw gdt_desc - gdt_start - 1
    dd gdt_start

