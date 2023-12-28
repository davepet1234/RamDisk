/**

 RamDisk.c

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/RamDisk.h>
#include <Protocol/DevicePathToText.h>
#include "CmdLineLib/CmdLine.h"

// Parameter variables
#define STR_MAXSIZE 20
CHAR16  Filename[STR_MAXSIZE];

// Main program help
CHAR16 ProgHelpStr[]    = L"Create or Load RAM disk";

// Switch table
SWTABLE_START(SwitchTable)
SWTABLE_MAN_STR(    L"-l",  L"-load",       Filename, STR_MAXSIZE,   L"[filename]Disk image filename")
SWTABLE_END

INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
    EFI_STATUS               Status;
    EFI_RAM_DISK_PROTOCOL    *MyRamDisk;
    UINT64                   *StartingAddr;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_FILE_HANDLE          FileHandle;
    EFI_FILE_INFO            *FileInfo;  
    UINTN                    ReadSize; 

    SHELL_STATUS ShellStatus = SHELL_SUCCESS;

    Filename[0] = '\0'        ;

    // Parse the command line 
    ShellStatus = ParseCmdLine(NULL, 0,SwitchTable, ProgHelpStr, NO_OPT, NULL);
    if (ShellStatus == SHELL_ABORTED){
        return ShellStatus;
    }

    Print(L"RamDisk application\n");
    Print(L"Filename: \"%s\"\n", Filename);
     
    // Look for Ram Disk Protocol
    Status = gBS->LocateProtocol(&gEfiRamDiskProtocolGuid, NULL, (VOID **)&MyRamDisk);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Couldn't find RamDiskProtocol\n");
        return EFI_ALREADY_STARTED;
    }

    //Open Image file and load it to the memory
    Status = ShellOpenFileByName(Filename, (SHELL_FILE_HANDLE *)&FileHandle, EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR (Status)) {
        Print(L"ERROR: OpenFile failed! Error=[%r]\n",Status);
        return EFI_SUCCESS;
    }                                                        

    //Get file size
    FileInfo = ShellGetFileInfo((SHELL_FILE_HANDLE)FileHandle);    
    
    //Allocate a memory for Image
    Status = gBS->AllocatePool(EfiReservedMemoryType, (UINTN)FileInfo-> FileSize, (VOID**)&StartingAddr                ); 
    if(EFI_ERROR(Status)) {
        Print(L"Allocate Memory failed!\n");
        return EFI_SUCCESS;
    } 
    
    //Load the whole file to the buffer
    Status = ShellReadFile(FileHandle,&ReadSize,StartingAddr);
    if(EFI_ERROR (Status)) {
        Print(L"Read file failed!\n");
        return EFI_SUCCESS;
} 
    
    //
    // Register the newly created RAM disk.
    //
    Status = MyRamDisk->Register(((UINT64)(UINTN) StartingAddr), FileInfo-> FileSize, &gEfiVirtualDiskGuid, NULL, &DevicePath);
    if (EFI_ERROR (Status)) {
        Print(L"Can't create RAM Disk!\n");
        return EFI_SUCCESS;
}

    //Show RamDisk DevicePath
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* Device2TextProtocol;
    CHAR16 *TextDevicePath = 0;
    Status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, (VOID**)&Device2TextProtocol);
    TextDevicePath = Device2TextProtocol->ConvertDevicePathToText(DevicePath, FALSE, TRUE); 
    Print(L"DevicePath=%s\n", TextDevicePath);
    Print(L"Disk Size =%d Bytes\n", FileInfo-> FileSize);

    if(TextDevicePath) {
        gBS->FreePool(TextDevicePath);
    }

    SHELL_FREE_NON_NULL(FileInfo);
    
    Print(L"Created Ram Disk success!\n");
    
    return 0;
}
