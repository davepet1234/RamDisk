/***********************************************************************

 RamDisk.c
 
 Author: David Petrovic
 GitHub: https://github.com/davepet1234/RamDisk

***********************************************************************/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/RamDisk.h>
#include <Library/DevicePathLib.h>
#include "CmdLineLib/CmdLine.h"
#include "Fat16.h"

// Parameter variables
#define STR_MAXSIZE 20
CHAR16  Filename[STR_MAXSIZE];
UINT32 DiskMBSize;

// Main program help
CHAR16 ProgHelpStr[]    = L"Create or Load RAM disk";

// Switch table
SWTABLE_START(SwitchTable)
SWTABLE_OPT_INT32(  L"-c",  L"-create",     &DiskMBSize,             L"[size]create RAM disk (size in MB)")
SWTABLE_OPT_STR(    L"-l",  L"-load",       Filename, STR_MAXSIZE,   L"[filename]Disk image filename")
SWTABLE_END

INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
    SHELL_STATUS ShellStatus = SHELL_SUCCESS;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_RAM_DISK_PROTOCOL *RamDiskProtocol = NULL;
    UINT64 *DiskStartAddr = NULL;
    UINT32 DiskByteSize = 0;
    BOOLEAN RamDiskCreated = FALSE;

    // Cmd line vars
    DiskMBSize = 0;
    Filename[0] = '\0';

    // Parse the command line 
    ShellStatus = ParseCmdLine(NULL, 0,SwitchTable, ProgHelpStr, NO_OPT, NULL);
    if (ShellStatus != SHELL_SUCCESS){
        goto Error_exit;
    }
    if (DiskMBSize && Filename[0]) {
        Print(L"ERROR: Please specify either create or load!\n");
        ShellStatus = SHELL_INVALID_PARAMETER;
        goto Error_exit;
    }

    Print(L"EFI RamDisk\n");
     
    // Locate Ram Disk Protocol
    Status = gBS->LocateProtocol(&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDiskProtocol);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: RamDiskProtocol not found (%r)!\n", Status);
        goto Error_exit;
    }

    if (DiskMBSize) {
        //---------------------------
        // Create blank RAM disk
        //---------------------------
        DiskByteSize = DiskMBSize * 1024 * 1024;

        Print(L"Disk Size: %d Bytes\n", DiskByteSize);
        Status = gBS->AllocatePool(EfiReservedMemoryType, (UINTN)DiskByteSize, (VOID**)&DiskStartAddr); 
        if(EFI_ERROR(Status)) {
            Print(L"ERROR: Failed to allocate memory (%r)!\n", Status);
            goto Error_exit;
        } 

        Print(L"Format RAM disk\n");
        Status = DiskFormatFat16((EFI_PHYSICAL_ADDRESS)DiskStartAddr, DiskByteSize);
        if (EFI_ERROR(Status)) {
            Print(L"ERROR: Failed to format disk (%r)!\n", Status);
            goto Error_exit;
        }

    } else if (Filename[0]) {
        //---------------------------
        // Create RAM disk from file
        //---------------------------
        Print(L"Filename: \"%s\"\n", Filename);
        EFI_FILE_HANDLE FileHandle;
        Status = ShellOpenFileByName(Filename, (SHELL_FILE_HANDLE *)&FileHandle, EFI_FILE_MODE_READ, 0);
        if(EFI_ERROR (Status)) {
            Print(L"ERROR: Failed to open file (%r)\n",Status);
            goto Error_exit;
        }                                                        

        EFI_FILE_INFO *FileInfo = ShellGetFileInfo((SHELL_FILE_HANDLE)FileHandle);
        if (!FileInfo) {
            Print(L"ERROR: Failed to get file info!\n");
            Status = EFI_NOT_FOUND;
            goto Error_exit;
        }
        DiskByteSize = FileInfo-> FileSize;
        SHELL_FREE_NON_NULL(FileInfo);

        Print(L"Disk Size: %d Bytes\n", DiskByteSize);
        Status = gBS->AllocatePool(EfiReservedMemoryType, (UINTN)DiskByteSize, (VOID**)&DiskStartAddr); 
        if(EFI_ERROR(Status)) {
            Print(L"ERROR: Failed to allocate memory (%r)!\n", Status);
            goto Error_exit;
        } 
        
        UINTN ReadSize = DiskByteSize; 
        Status = ShellReadFile(FileHandle, &ReadSize, DiskStartAddr);
        if(EFI_ERROR (Status)) {
            Print(L"ERROR: Failed to read file (%r)!\n", Status);
            goto Error_exit;
        }
        if(ReadSize != DiskByteSize) {
            Print(L"ERROR: Failed to read whole file (%u/%u)!\n", ReadSize, DiskByteSize);
            Status = EFI_DEVICE_ERROR;
            goto Error_exit;
        } 
    }

    if (DiskByteSize) {
        //---------------------------
        // Register RAM disk
        //---------------------------
        Print(L"Register RAM disk\n");
        EFI_DEVICE_PATH_PROTOCOL *DevicePath;
        Status = RamDiskProtocol->Register(((UINT64)(UINTN) DiskStartAddr), DiskByteSize, &gEfiVirtualDiskGuid, NULL, &DevicePath);
        if (EFI_ERROR (Status)) {
            Print(L"ERROR: Failed to create RAM Disk (%r)!\n", Status);
            goto Error_exit;
        }
        RamDiskCreated = TRUE;

        // Show RamDisk DevicePath
        CHAR16 *TextDevicePath = ConvertDevicePathToText(DevicePath, FALSE, TRUE);
        if (TextDevicePath) {
            Print(L"DevicePath: %s\n", TextDevicePath);    
        } else {
            Print(L"ERROR: Failed to get device path!\n");            
        }
        SHELL_FREE_NON_NULL(TextDevicePath);
    }

Error_exit:
    if (RamDiskCreated) {
        Print(L"Successfully created Ram disk!\n");
    } else {
        SHELL_FREE_NON_NULL(DiskStartAddr);
    }

    return EFI_ERROR(Status) ? Status & (~MAX_BIT) : ShellStatus;
}
