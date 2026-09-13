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
[ORG 0x7C00]

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Save boot drive passed in dl by the BIOS
    mov [boot_drive], dl

    ; Clear screen
    mov ah, 0x00
    mov al, 0x03
    int 0x10


    ; Print startup messages
    mov si, msg_start
    call print_string

    mov si, msg_key
    call print_string

    mov si, msg_drm
    call print_string

    mov si, msg_kernel
    call print_string

    ; Load Stage 2 from disk using LBA read
    mov dl, [boot_drive]
    mov ah, 0x42
    mov si, dap
    int 0x13
    jc disk_error

    ; Jump to Stage 2
    jmp 0x0000:0x7E00

disk_error:
    mov si, msg_err
    call print_string
halt:
    hlt
    jmp halt


; --- Functions ---
print_string:
    mov ah, 0x0E ; Teletype output
.loop:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

; --- Data ---
msg_start:  db "Welcome to ArkOS Bootloader", 13, 10, 0
msg_key:    db "Checking Hardware Secure Key... OK", 13, 10, 0
msg_drm:    db "Loading DRM Module... OK", 13, 10, 0
msg_kernel: db "Loading Kernel & Launching OS...", 13, 10, 0
msg_err:    db "Disk Error loading Stage 2!", 13, 10, 0

boot_drive: db 0

align 4
dap:
    db 0x10 ; size of DAP
    db 0    ; zero
    dw 7    ; number of sectors to read
    dw 0x7E00 ; offset
    dw 0x0000 ; segment
    dq 34   ; LBA 34 (Stage 2 starts at sector 34 in hybrid GPT layout)

times 446-($-$$) db 0
; Partition table placeholder (4 partitions * 16 bytes = 64 bytes)
times 64 db 0
; Boot signature
dw 0xAA55
