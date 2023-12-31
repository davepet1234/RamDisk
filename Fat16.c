/***********************************************************************

 Fat16.c
 
 Author: David Petrovic
 GitHub: https://github.com/davepet1234/RamDisk

 Ref:   Microsoft FAT Specification 
        Microsoft Corporation 
        August 30 2005 

***********************************************************************/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include "Library/BaseMemoryLib/MemLibInternals.h"
#include <Library/DebugLib.h>

// FAT16 Boot Sector and BPB (BIOS Parameter Block)
#pragma pack (push, 1)
typedef struct {        // Offset (byte) - Description
	UINT8  		BS_jmpBoot[3];          // 0  - Jump instruction to boot code.
	UINT8  		BS_OEMName[8];          // 3  - OEM Name Identifier.
	UINT16 		BPB_BytsPerSec;         // 11 - Count of bytes per sector.
	UINT8  		BPB_SecPerClus;         // 13 - Number of sectors per allocation unit.
	UINT16 		BPB_RsvdSecCnt;         // 14 - Number of reserved sectors in the reserved region of the volume starting at the first sector of the volume.
	UINT8  		BPB_NumFATs;            // 16 - The count of file allocation tables (FATs) on the volume.
	UINT16 		BPB_RootEntCnt;         // 17 - For FAT12 and FAT16 volumes, this field contains the count of 32-byte directory entries in the root directory.
	UINT16 		BPB_TotSec16;           // 19 - This field is the old 16-bit total count of sectors on the volume.
	UINT8  		BPB_Media;              // 21 - The legal values for this field are 0xF0, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, and 0xFF.
	UINT16 		BPB_FATSz16;            // 22 - This field is the FAT12/FAT16 16-bit count of sectors occupied by one FAT.
	UINT16 		BPB_SecPerTrk;          // 24 - Sectors per track for interrupt 0x13.
	UINT16 		BPB_NumHeads;           // 26 - Number of heads for interrupt 0x13. This field is relevant as discussed earlier for BPB_SecPerTrk. 
	UINT32 		BPB_HiddSec;            // 28 - Count of hidden sectors preceding the partition that contains this FAT volume.
	UINT32 		BPB_TotSec32;           // 32 - This field is the new 32-bit total count of sectors on the volume.
	UINT8  		BS_DrvNum;              // 36 - Interrupt 0x13 drive number. Set value to 0x80 or 0x00.
	UINT8  		BS_Reserved1;           // 37 - Reserved.
	UINT8  		BS_BootSig;             // 38 - Extended boot signature.
	UINT32 		BS_VolID;               // 39 - Volume serial number.
	UINT8  		BS_VolLab[11];          // 43 - Volume label.
	UINT8  		BS_FilSysType[8];       // 54 - One of the strings “FAT12 ”, “FAT16 ”, or “FAT ”.
    UINT8  		BS_Code[448];           // 62 - Set to 0x00.
	UINT16 		BS_Sig;                 // 510 - Set to 0x55 (at byte offset 510) and 0xAA (at byte offset 511).
} BOOT_SECTOR_FAT16;
#pragma pack(pop)

// This table is for FAT 16 drives and defines the sectors per cluster given a specific disk size.
// For 512 byte sector sized media: if volume size is < 512 MB, the volume is formatted 
// FAT16 else it is formatted FAT32. It is possible to override the default FAT type selection. 
typedef struct {
    UINT32 DiskSize; 
    UINT8 SecPerClusVal; 
 } DSKSZTOSECPERCLUS;

STATIC DSKSZTOSECPERCLUS DskTableFAT16[] = { 
    {8400, 0},      // disks up to 4.1 MB, the 0 value for SecPerClusVal trips an error 
    {32680, 2},     // disks up to 16 MB, 1k cluster
    {262144, 4},    // disks up to 128 MB, 2k cluster
    {524288, 8},    // disks up to 256 MB, 4k cluster
    {1048576, 16},  // disks up to 512 MB, 8k cluster
    // The entries after this point are not used unless FAT16 is forced
    {2097152, 32},  // disks up to 1 GB, 16k cluster
    {4194304, 64},  // disks up to 2 GB, 32k cluster
    {0xFFFFFFFF, 0} // any disk greater than 2GB, 0 value for SecPerClusVal trips an error
};

/*
 * DskSzToSecPerClus()
 *
 * Given a disk size, this function determines the BPB_SecPerClus value. 
 */
STATIC UINT8 DskSzToSecPerClus(IN UINT32 DiskSectors)
{
    UINT8   i = 0;
    while( DskTableFAT16[i++].DiskSize != 0xFFFFFFFF ) {    
        if (DiskSectors <= DskTableFAT16[i].DiskSize) {
            return DskTableFAT16[i].SecPerClusVal;
        }
    }
    return 0;
}

/*
 * DiskFormatFat16()
 */

EFI_STATUS DiskFormatFat16(IN CONST EFI_PHYSICAL_ADDRESS DiskStart, IN CONST UINT32 DiskSize)
{
    SetMem((VOID *)DiskStart, DiskSize, 0);

    BOOT_SECTOR_FAT16 *pDiskBS = (BOOT_SECTOR_FAT16 *)DiskStart;

    // Initilize Master Boot Sector
    pDiskBS->BS_jmpBoot[0] = 0xEB;
    pDiskBS->BS_jmpBoot[1] = 0x00;
    pDiskBS->BS_jmpBoot[2] = 0x90;
    
    CopyMem(pDiskBS->BS_OEMName, "EFI RAM ", 8);
    
    pDiskBS->BPB_BytsPerSec    = 512;
    pDiskBS->BPB_SecPerClus    = 0;
    pDiskBS->BPB_RsvdSecCnt    = 1;
    pDiskBS->BPB_NumFATs       = 2;
    pDiskBS->BPB_RootEntCnt    = 512;
    pDiskBS->BPB_TotSec16      = 0;
    pDiskBS->BPB_Media         = 0xF8; // "fixed" (non-removable) media
    pDiskBS->BPB_FATSz16       = 0;
    pDiskBS->BPB_SecPerTrk     = 0;
    pDiskBS->BPB_NumHeads      = 0;
    pDiskBS->BPB_HiddSec       = 0;
    pDiskBS->BPB_TotSec32      = 0;
    pDiskBS->BS_DrvNum         = 0;
    pDiskBS->BS_Reserved1      = 0;
    pDiskBS->BS_BootSig        = 0x29;
    pDiskBS->BS_VolID          = 0;
    CopyMem(pDiskBS->BS_VolLab, "RAMDISK    ", 11);
    CopyMem(pDiskBS->BS_FilSysType, "FAT16   ", 8);
    pDiskBS->BS_Sig            = 0xAA55;
  
    // Determine total sectors on disk
    UINT32 DiskSzInSectors = ( DiskSize / pDiskBS->BPB_BytsPerSec);
    if (DiskSzInSectors > 0xFFFF) {
        pDiskBS->BPB_TotSec32 = DiskSzInSectors;
    } else {
        pDiskBS->BPB_TotSec16 = DiskSzInSectors;
    }
  
    // Determine sectors per cluster
    pDiskBS->BPB_SecPerClus = DskSzToSecPerClus(DiskSzInSectors);
    //Print(L"pDiskBS->BPB_SecPerClus = %d\n", pDiskBS->BPB_SecPerClus);
    ASSERT( pDiskBS->BPB_SecPerClus != 0);
    
    // Determine number of sectors per FAT
    UINT32 RootDirSectors = ((pDiskBS->BPB_RsvdSecCnt * 32) + (pDiskBS->BPB_BytsPerSec - 1)) / pDiskBS->BPB_BytsPerSec; // Sectors needed for Root Directory
    UINT32 TempVal1 = DiskSzInSectors - (pDiskBS->BPB_RsvdSecCnt + RootDirSectors);
    UINT32 TempVal2 = (256 * pDiskBS->BPB_SecPerClus) + pDiskBS->BPB_NumFATs;
    UINT32 SectorsPerFat = (TempVal1 + (TempVal2 - 1)) / TempVal2;
    if (SectorsPerFat > 0xFFFF) {
        return EFI_INVALID_PARAMETER;
    }
    pDiskBS->BPB_FATSz16 = (UINT16)SectorsPerFat;
  
    // Determine FAT Type by count of clusters on the volume
    UINT32 DataSectors = DiskSzInSectors - ( pDiskBS->BPB_RsvdSecCnt + (pDiskBS->BPB_NumFATs * SectorsPerFat) + RootDirSectors);
    UINT32 DiskSzInClusters = DataSectors / pDiskBS->BPB_SecPerClus;
    if (DiskSzInClusters < 4085) {
        // Should be FAT12
        Print(L"ERROR: Clusters count to small for FAT16 (%u)!\n", DiskSzInClusters);
        return EFI_UNSUPPORTED;
    } else if (DiskSzInClusters >= 65525 ) {
        // Should be FAT32
        Print(L"ERROR: Clusters count to large for FAT16 (%u)!\n", DiskSzInClusters);
        return EFI_UNSUPPORTED;
    }
  
    // Initialize FATs
    UINT8 *Fat1 = (UINT8*)pDiskBS + pDiskBS->BPB_RsvdSecCnt * 512;
    UINT8 *Fat2 = (UINT8*)pDiskBS + (pDiskBS->BPB_RsvdSecCnt + pDiskBS->BPB_FATSz16) * 512;
    // Initialize FAT1
    Fat1[0] = pDiskBS->BPB_Media;
    Fat1[1] = 0xFF;
    Fat1[2] = 0xFF;
    Fat1[3] = 0xFF;
    // Initialize FAT2
    Fat2[0] = pDiskBS->BPB_Media;
    Fat2[1] = 0xFF;
    Fat2[2] = 0xFF;
    Fat2[3] = 0xFF;

    return EFI_SUCCESS;
}
