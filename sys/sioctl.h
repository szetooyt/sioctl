/*++

Copyright (c) 1997  Microsoft Corporation

Module Name:

    SIOCTL.H

Abstract:


    Defines the IOCTL codes that will be used by this driver.  The IOCTL code
    contains a command identifier, plus other information about the device,
    the type of access with which the file must have been opened,
    and the type of buffering.

Environment:

    Kernel mode only.

--*/

#include "..\exe\ioctlcode.h"

#define WPP_CONTROL_GUIDS                                            \
    WPP_DEFINE_CONTROL_GUID( FileIoTraceGuid,                        \
                             (71ae54db,0862,41bf,a24f,5330cec3c888), \
                             WPP_DEFINE_BIT(FLAG_ONE)     \
                             WPP_DEFINE_BIT(FLAG_TWO)       \
                             )  


#define NT_DEVICE_NAME2      L"\\Device\\Ramdisk"
#define DOS_DEVICE_NAME2     L"\\DosDevices\\"		// to be append with L"B:"

#define RAMDISK_TAG_GENERAL             '1maR'  // "Ram1" - generic tag
#define RAMDISK_TAG_DISK                '2maR'  // "Ram2" - disk memory tag

// #define DRIVE_LETTER_LENGTH             (sizeof(WCHAR)*10)


#define RAMDISK_DRIVER_EXTENSION_KEY    ((PVOID) DriverEntry)
#define RAMDISK_MEDIA_TYPE              RemovableMedia
//FixedMedia	//0xF8
// used only in formatdisk, in rounding and fatentries computation
// #define DIR_ENTRIES_PER_SECTOR          16

#define FLAG_LINK_CREATED               0x00000001
#define FLAG_VOL_DEVICE                 0x00000002

#define REMLOCK_TAG                     'lmaR'
#define REMLOCK_MAXIMUM                 1        // Max minutes system allows lock to be held
#define REMLOCK_HIGHWATER               10       // Max number of irps holding lock at one time


// #if DBG
// #define DEFAULT_BREAK_ON_ENTRY          0                   // No break
// #define DEFAULT_DEBUG_LEVEL             (DBG_LEVEL_ERROR)   // Log only errors
// #define DEFAULT_DEBUG_COMP              (DBG_COMP_ALL)
// #endif

// #define DEFAULT_DISK_SIZE               1023809024    // 1 MB (1024*1024) 
// #define DEFAULT_ROOT_DIR_ENTRIES        512
// #define DEFAULT_SECTORS_PER_CLUSTER     2
// #define DEFAULT_DRIVE_LETTER            L"B:"

typedef enum  _DEVICE_STATE {
    STOPPED,                    // Dvice stopped
    WORKING,                    // Started and working
    PENDINGSTOP,                // Stop pending
    PENDINGREMOVE,              // Remove pending
    SURPRISEREMOVED,            // Surprise removed
    REMOVED,                    // Removed
    MAX_STATE                   // Unknown state -Some error
} DEVICE_STATE, *PDEVICE_STATE;

/******
typedef struct _DISK_INFO {
    ULONG   DiskSize;           // Ramdisk size in bytes
    ULONG   RootDirEntries;     // No. of root directory entries
    ULONG   SectorsPerCluster;  // Sectors per cluster
    UNICODE_STRING DriveLetter; // Drive letter to be used
} DISK_INFO, *PDISK_INFO;
************/
typedef struct _RAMDISK_DRIVER_EXTENSION {
    UNICODE_STRING  RegistryPath;
	ULONG           DeviceInitialized;
} RAMDISK_DRIVER_EXTENSION, *PRAMDISK_DRIVER_EXTENSION;

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT      DeviceObject;               // Back pointer to device object
    PDEVICE_OBJECT      LowerDeviceObject;          // Target device object
    PDEVICE_OBJECT      PhysicalDeviceObject;       // Physica device object
    DEVICE_STATE        DevState;                   // Current device state
    IO_REMOVE_LOCK      RemoveLock;                 // Remove lock to avoid abnormal device removal
// field to differenciate dummy device ?
// retain filename in devext so that there is no need to find it later
// maintain io counts
    ULONG               Flags;                      // General device flag
    HANDLE		FileHandle;		    // our volume
//    PUCHAR              DiskImage;                  // Pointer to beginning of disk image
//    DISK_GEOMETRY       DiskGeometry;               // Drive parameters built by Ramdisk
//    DISK_INFO           DiskRegInfo;                // Disk parameters from the registry
    LARGE_INTEGER	DiskSize;
    USHORT	Cylinders;
    USHORT	TracksPerCyl;
    USHORT	SectorsPerTrk;
    USHORT	BytesPerSec;
    WCHAR	DriveLetter[3];
    USHORT	nonloop;
    ULONG	ReadCount;			//for reporting stats count
    ULONG	WriteCount;			//for reporting stats count
//    BOOLEAN	startPrint;				//show before/after looping
//    BOOLEAN	donePrint;				//only print 1 block
    UNICODE_STRING      SymbolicLink;               // Dos symbolic name; Drive letter
    
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;



#pragma pack(1)
/*************************************
typedef struct  _BOOT_SECTOR
{
    UCHAR       bsJump[3];          // x86 jmp instruction, checked by FS
    CCHAR       bsOemName[8];       // OEM name of formatter
    USHORT      bsBytesPerSec;      // Bytes per Sector
    UCHAR       bsSecPerClus;       // Sectors per Cluster
    USHORT      bsResSectors;       // Reserved Sectors
    UCHAR       bsFATs;             // Number of FATs - we always use 1
    USHORT      bsRootDirEnts;      // Number of Root Dir Entries
    USHORT      bsSectors;          // Number of Sectors
    UCHAR       bsMedia;            // Media type - we use RAMDISK_MEDIA_TYPE
    USHORT      bsFATsecs;          // Number of FAT sectors
    USHORT      bsSecPerTrack;      // Sectors per Track - we use 32
    USHORT      bsHeads;            // Number of Heads - we use 2
    ULONG       bsHiddenSecs;       // Hidden Sectors - we set to 0
    ULONG       bsHugeSectors;      // Number of Sectors if > 32 MB size
    UCHAR       bsDriveNumber;      // Drive Number - not used
    UCHAR       bsReserved1;        // Reserved
    UCHAR       bsBootSignature;    // New Format Boot Signature - 0x29
    ULONG       bsVolumeID;         // VolumeID - set to 0x12345678
    CCHAR       bsLabel[11];        // Label - set to RamDisk
    CCHAR       bsFileSystemType[8];// File System Type - FAT12 or FAT16
    CCHAR       bsReserved2[448];   // Reserved
    UCHAR       bsSig2[2];          // Originial Boot Signature - 0x55, 0xAA
}   BOOT_SECTOR, *PBOOT_SECTOR;

typedef struct  _DIR_ENTRY
{
    UCHAR       deName[8];          // File Name
    UCHAR       deExtension[3];     // File Extension
    UCHAR       deAttributes;       // File Attributes
    UCHAR       deReserved;         // Reserved
    USHORT      deTime;             // File Time
    USHORT      deDate;             // File Date
    USHORT      deStartCluster;     // First Cluster of file
    ULONG       deFileSize;         // File Length
}   DIR_ENTRY, *PDIR_ENTRY;
******************/
#pragma pack()

#define COMPLETE_REQUEST( _pIrp, _Status, _Information )    \
        {                                                   \
            ASSERT( _pIrp != NULL );                        \
            ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL ); \
            _pIrp->IoStatus.Status = _Status;               \
            _pIrp->IoStatus.Information = _Information;     \
            IoCompleteRequest( _pIrp, IO_NO_INCREMENT );    \
        }
//
// Directory Entry Attributes
//

#define DIR_ATTR_READONLY   0x01
#define DIR_ATTR_HIDDEN     0x02
#define DIR_ATTR_SYSTEM     0x04
// used in Format disk. for first directory entry, the rest of DIR_ATTR_ not used anywhere else
#define DIR_ATTR_VOLUME     0x08
#define DIR_ATTR_DIRECTORY  0x10
#define DIR_ATTR_ARCHIVE    0x20


