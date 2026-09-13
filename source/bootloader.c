//
// Copyright 2026 Aarav Ravindra Kharade
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#include <efi.h>
#include <efilib.h>

EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_ID;

UINTN StrLen(const CHAR16 *s) {
    UINTN len = 0;
    while (s[len] != 0) len++;
    return len;
}

// Simulated hardware secure key
#define HARDWARE_SECURE_KEY "ARK-OS-9aaafa37077ee91e5df2f47a8e9e1564beecde1cc13b279f51d915bf8c746eb4"

EFI_STATUS check_os_signature(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Conceptual hardware verified boot check
    int has_key = 1;
    int match = 1;

    if (has_key) {
        if (match) {
            return EFI_SUCCESS;
        } else {
            return EFI_SECURITY_VIOLATION;
        }
    } else {
        return EFI_SUCCESS;
    }
}

EFI_STATUS load_drm_module() {
    // Conceptual DRM module loading
    return EFI_SUCCESS;
}

static EFI_STATUS read_file_from_esp(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    CHAR16 *FileName,
    VOID **BufferOut,
    UINTN *SizeOut
) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_HANDLE root = NULL;
    EFI_FILE_HANDLE file = NULL;

    // 1. Get the LoadedImage protocol for the current bootloader
    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **)&loaded_image
    );
    if (EFI_ERROR(status)) return status;

    // 2. Open the SimpleFileSystem protocol on the device handle of the loaded image
    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        loaded_image->DeviceHandle,
        &FileSystemProtocol,
        (VOID **)&fs
    );
    if (EFI_ERROR(status)) return status;

    // 3. Open the volume
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) return status;

    // 4. Open the file
    status = uefi_call_wrapper(
        root->Open,
        5,
        root,
        &file,
        FileName,
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }

    // 5. Get file info to determine size
    EFI_FILE_INFO *file_info = NULL;
    UINTN info_size = 0;
    status = uefi_call_wrapper(
        file->GetInfo,
        4,
        file,
        &GenericFileInfo,
        &info_size,
        NULL
    );
    if (status == EFI_BUFFER_TOO_SMALL) {
        status = uefi_call_wrapper(
            SystemTable->BootServices->AllocatePool,
            3,
            EfiLoaderData,
            info_size,
            (VOID **)&file_info
        );
        if (EFI_ERROR(status)) {
            uefi_call_wrapper(file->Close, 1, file);
            uefi_call_wrapper(root->Close, 1, root);
            return status;
        }

        status = uefi_call_wrapper(
            file->GetInfo,
            4,
            file,
            &GenericFileInfo,
            &info_size,
            (VOID *)file_info
        );
    }

    if (EFI_ERROR(status)) {
        if (file_info) uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, file_info);
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }

    UINTN file_size = file_info->FileSize;
    uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, file_info);

    // 6. Allocate buffer for file contents using AllocatePages for large buffers
    UINTN num_pages = (file_size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS phys_buffer = 0;
    status = uefi_call_wrapper(
        SystemTable->BootServices->AllocatePages,
        4,
        AllocateAnyPages,
        EfiLoaderData,
        num_pages,
        &phys_buffer
    );
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }
    VOID *buffer = (VOID *)(UINTN)phys_buffer;

    // 7. Read file contents
    UINTN read_size = file_size;
    status = uefi_call_wrapper(file->Read, 3, file, &read_size, buffer);
    if (EFI_ERROR(status) || read_size != file_size) {
        uefi_call_wrapper(SystemTable->BootServices->FreePages, 2, phys_buffer, num_pages);
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_DEVICE_ERROR;
    }

    // 8. Clean up and return
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(root->Close, 1, root);

    *BufferOut = buffer;
    *SizeOut = file_size;
    return EFI_SUCCESS;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;

    // 1. Locate the Graphics Output Protocol (GOP)
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    status = uefi_call_wrapper(
        SystemTable->BootServices->LocateProtocol,
        3,
        &gop_guid,
        NULL,
        (VOID **)&gop
    );

    if (!EFI_ERROR(status) && gop) {
        UINT32 max_mode = gop->Mode->MaxMode;
        UINT32 target_mode = gop->Mode->Mode;
        UINT32 max_res = 0;

        for (UINT32 i = 0; i < max_mode; i++) {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
            UINTN size_of_info;
            EFI_STATUS s = uefi_call_wrapper(gop->QueryMode, 4, gop, i, &size_of_info, &info);
            if (!EFI_ERROR(s)) {
                if (info->HorizontalResolution == 3840 && info->VerticalResolution == 2160) {
                    target_mode = i;
                    uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, info);
                    break;
                }
                UINT32 res = info->HorizontalResolution * info->VerticalResolution;
                if (res > max_res) {
                    max_res = res;
                    target_mode = i;
                }
                uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, info);
            }
        }
        uefi_call_wrapper(gop->SetMode, 2, gop, target_mode);
    }

    UINTN center_x = 0;
    UINTN center_y = 0;
    UINTN anim_width = 200;
    UINTN anim_height = 200;

    // 2. Clear screen to black and display the white dot
    if (!EFI_ERROR(status) && gop) {
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL black = {0, 0, 0, 0};
        uefi_call_wrapper(
            gop->Blt,
            10,
            gop,
            &black,
            EfiBltVideoFill,
            (UINTN)0, (UINTN)0,
            (UINTN)0, (UINTN)0,
            gop->Mode->Info->HorizontalResolution,
            gop->Mode->Info->VerticalResolution,
            (UINTN)0
        );

        center_x = (gop->Mode->Info->HorizontalResolution - anim_width) / 2;
        center_y = (gop->Mode->Info->VerticalResolution - anim_height) / 2;

        // Draw a 6x6 pixel white dot in the center of the 200x200 region
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL white = {255, 255, 255, 0};
        uefi_call_wrapper(
            gop->Blt,
            10,
            gop,
            &white,
            EfiBltVideoFill,
            (UINTN)0, (UINTN)0,
            center_x + 97, center_y + 97,
            (UINTN)6, (UINTN)6,
            (UINTN)0
        );
    }

    // 3. Perform OS Key signature check and load DRM
    status = check_os_signature(ImageHandle, SystemTable);
    if (EFI_ERROR(status)) {
        return status;
    }
    load_drm_module();

    // 4. Resolve path and load the kernel image (bzImage)
    EFI_LOADED_IMAGE *loaded_image = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **)&loaded_image
    );
    if (EFI_ERROR(status)) return status;

    VOID *kernel_buffer = NULL;
    UINTN kernel_size = 0;
    status = read_file_from_esp(ImageHandle, SystemTable, L"\\EFI\\BOOT\\bzImage", &kernel_buffer, &kernel_size);
    if (EFI_ERROR(status)) return status;

    EFI_HANDLE kernel_img = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->LoadImage,
        6,
        FALSE,
        ImageHandle,
        NULL,
        kernel_buffer,
        kernel_size,
        &kernel_img
    );
    if (EFI_ERROR(status)) return status;

    // 5. Play the boot animation (expanding nucleus) from ESP
    //    If animation fails to load, skip gracefully — never hang.
    if (gop) {
        VOID *anim_buffer = NULL;
        UINTN anim_size = 0;
        status = read_file_from_esp(ImageHandle, SystemTable, L"\\EFI\\BOOT\\animation.bin", &anim_buffer, &anim_size);
        if (EFI_ERROR(status)) {
            // Animation file missing — skip animation, proceed to kernel boot
            // Show the white dot briefly as visual feedback then continue
            uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 500000); // 0.5s
        } else {
            UINTN num_frames = 60;
            UINTN expected_size = num_frames * anim_width * anim_height * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
            
            if (anim_size < expected_size) {
                // Animation file corrupted/wrong size — skip, don't hang
                uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 500000);
            } else {
                // Clear the white dot to black first
                EFI_GRAPHICS_OUTPUT_BLT_PIXEL black = {0, 0, 0, 0};
                uefi_call_wrapper(
                    gop->Blt,
                    10,
                    gop,
                    &black,
                    EfiBltVideoFill,
                    (UINTN)0, (UINTN)0,
                    (UINTN)0, (UINTN)0,
                    gop->Mode->Info->HorizontalResolution,
                    gop->Mode->Info->VerticalResolution,
                    (UINTN)0
                );

                EFI_GRAPHICS_OUTPUT_BLT_PIXEL *frames = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)anim_buffer;
                for (UINTN f = 0; f < num_frames; f++) {
                    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *frame = &frames[f * anim_width * anim_height];
                    
                    uefi_call_wrapper(
                        gop->Blt,
                        10,
                        gop,
                        frame,
                        EfiBltBufferToVideo,
                        (UINTN)0, (UINTN)0,
                        center_x, center_y,
                        anim_width, anim_height,
                        anim_width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                    );
                    
                    uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 16666);
                }
            }
            uefi_call_wrapper(SystemTable->BootServices->FreePages, 2, (EFI_PHYSICAL_ADDRESS)(UINTN)anim_buffer, (anim_size + 4095) / 4096);
        }
        
        // Clear screen to black before handing off to kernel
        // This prevents leftover animation pixels from persisting
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL final_black = {0, 0, 0, 0};
        uefi_call_wrapper(
            gop->Blt, 10, gop, &final_black, EfiBltVideoFill,
            (UINTN)0, (UINTN)0, (UINTN)0, (UINTN)0,
            gop->Mode->Info->HorizontalResolution,
            gop->Mode->Info->VerticalResolution, (UINTN)0
        );
        // Brief stall to ensure GOP flush completes before kernel takes over
        uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 50000);
    }

    // 6. Set command line options for the kernel
    EFI_LOADED_IMAGE *kernel_loaded_image = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        kernel_img,
        &LoadedImageProtocol,
        (VOID **)&kernel_loaded_image
    );
    if (EFI_ERROR(status)) return status;

    CHAR16 *cmd_line = L"initrd=\\EFI\\BOOT\\initramfs.img console=ttyS0 console=tty0 init=/init root=/dev/sdb rw vt.global_cursor_default=0";
    kernel_loaded_image->LoadOptions = cmd_line;
    kernel_loaded_image->LoadOptionsSize = (StrLen(cmd_line) + 1) * sizeof(CHAR16);

    // 7. Start the kernel
    UINTN exit_data_size = 0;
    CHAR16 *exit_data = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->StartImage,
        3,
        kernel_img,
        &exit_data_size,
        &exit_data
    );

    return status;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}
