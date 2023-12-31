/***********************************************************************

 FAt16.h
 
 Author: David Petrovic
 GitHub: https://github.com/davepet1234/RamDisk

***********************************************************************/

#ifndef FAT16_H
#define FAT16_H
#ifdef __cplusplus
extern "C" {
#endif

#include <Uefi.h>


EFI_STATUS DiskFormatFat16(IN CONST EFI_PHYSICAL_ADDRESS DiskStart, IN CONST UINT32 DiskSize);


#ifdef __cplusplus
}
#endif
#endif // FAT16_H