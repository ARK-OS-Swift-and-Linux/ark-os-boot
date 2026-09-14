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
    return EFI_SUCCESS;
}

EFI_STATUS load_drm_module() {
    return EFI_SUCCESS;
}

static void print(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *str) {
    uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, str);
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

    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **)&loaded_image
    );
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        loaded_image->DeviceHandle,
        &FileSystemProtocol,
        (VOID **)&fs
    );
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) return status;

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

    UINTN read_size = file_size;
    status = uefi_call_wrapper(file->Read, 3, file, &read_size, buffer);
    if (EFI_ERROR(status) || read_size != file_size) {
        uefi_call_wrapper(SystemTable->BootServices->FreePages, 2, phys_buffer, num_pages);
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_DEVICE_ERROR;
    }

    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(root->Close, 1, root);

    *BufferOut = buffer;
    *SizeOut = file_size;
    return EFI_SUCCESS;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;

    // Removed bootloader starting print

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
        // Just clear to black for testing without messing up text mode for now
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
    }

    EFI_LOADED_IMAGE *loaded_image = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **)&loaded_image
    );
    if (EFI_ERROR(status)) {
        print(SystemTable, L"Failed to get loaded image\r\n");
        return status;
    }

    // Removed kernel loading print
    VOID *kernel_buffer = NULL;
    UINTN kernel_size = 0;
    status = read_file_from_esp(ImageHandle, SystemTable, L"\\EFI\\BOOT\\x86_64", &kernel_buffer, &kernel_size);
    if (EFI_ERROR(status)) {
        print(SystemTable, L"Failed to load kernel\r\n");
        return status;
    }

    // Removed kernel loaded print

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
    if (EFI_ERROR(status)) {
        print(SystemTable, L"LoadImage failed\r\n");
        return status;
    }

    EFI_LOADED_IMAGE *kernel_loaded_image = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->HandleProtocol,
        3,
        kernel_img,
        &LoadedImageProtocol,
        (VOID **)&kernel_loaded_image
    );
    if (EFI_ERROR(status)) {
        print(SystemTable, L"Failed to get kernel loaded image\r\n");
        return status;
    }

    CHAR16 *cmd_line = L"initrd=\\EFI\\BOOT\\initramfs.img earlycon=uart8250,io,0x3f8,115200n8 console=ttyS0,115200 quiet loglevel=3 init=/init root=/dev/sdb rw vt.global_cursor_default=0";
    kernel_loaded_image->LoadOptions = cmd_line;
    kernel_loaded_image->LoadOptionsSize = (StrLen(cmd_line) + 1) * sizeof(CHAR16);
    kernel_loaded_image->DeviceHandle = loaded_image->DeviceHandle;

    // Play animation
    VOID *anim_buffer = NULL;
    UINTN anim_size = 0;
    EFI_STATUS anim_status = read_file_from_esp(ImageHandle, SystemTable, L"\\animation.bin", &anim_buffer, &anim_size);
    if (!EFI_ERROR(anim_status) && gop && anim_buffer) {
        UINT8 *frames = (UINT8 *)anim_buffer;
        UINTN frame_size = 200 * 200 * 4;
        UINTN num_frames = anim_size / frame_size;
        UINTN center_x = (gop->Mode->Info->HorizontalResolution - 200) / 2;
        UINTN center_y = (gop->Mode->Info->VerticalResolution - 200) / 2;
        
        for (UINTN i = 0; i < num_frames; i++) {
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL *frame_ptr = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)(frames + i * frame_size);
            uefi_call_wrapper(
                gop->Blt,
                10,
                gop,
                frame_ptr,
                EfiBltBufferToVideo,
                (UINTN)0, (UINTN)0,
                center_x, center_y,
                (UINTN)200, (UINTN)200,
                (UINTN)(200 * 4)
            );
            // 60 fps -> 16666 microseconds
            uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 16666);
        }
        
        UINTN num_anim_pages = (anim_size + 4095) / 4096;
        uefi_call_wrapper(SystemTable->BootServices->FreePages, 2, (EFI_PHYSICAL_ADDRESS)(UINTN)anim_buffer, num_anim_pages);
    }

    UINTN exit_data_size = 0;
    CHAR16 *exit_data = NULL;
    status = uefi_call_wrapper(
        SystemTable->BootServices->StartImage,
        3,
        kernel_img,
        &exit_data_size,
        &exit_data
    );

    if (EFI_ERROR(status)) {
        print(SystemTable, L"StartImage failed\r\n");
        uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 10000000);
    }

    return status;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}
