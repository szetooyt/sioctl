//
// Device type           -- in the "User Defined" range."
//
#define SIOCTL_TYPE 40000
//
// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//
#define IOCTL_SIOCTL_METHOD_STATS_COUNT \
    CTL_CODE( SIOCTL_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_SIOCTL_METHOD_UMOUNT \
    CTL_CODE( SIOCTL_TYPE, 0x901, METHOD_OUT_DIRECT , FILE_ANY_ACCESS  )

#define IOCTL_SIOCTL_METHOD_ADD_VOLUME  \
    CTL_CODE( SIOCTL_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define IOCTL_SIOCTL_VOLUME_ARRIVAL \
    CTL_CODE( SIOCTL_TYPE, 0x903, METHOD_NEITHER , FILE_ANY_ACCESS  )

#define IOCTL_SIOCTL_METHOD_BUFFERED  \
    CTL_CODE( SIOCTL_TYPE, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS  )


#define ACCESS_FROM_CTL_CODE(ctrlCode)     (((ULONG)(ctrlCode & 0x0000D000)) >> 14)
#define FUNCTION_FROM_CTL_CODE(ctrlCode)     (((ULONG)(ctrlCode & 0x00003FFD)) >> 2)


#define DRIVER_FUNC_INSTALL     0x01
#define DRIVER_FUNC_REMOVE      0x02

#define DRIVER_NAME       "SIoctl"

#define MAX_PATH2          80			//in windef.h

#pragma pack(1)

// follow DISK_GEOMETRY_EX
typedef struct _CREATE_VOL {
    LARGE_INTEGER	DiskSize;		//written to vol
    USHORT	Cylinders;			//written to vol
    USHORT	TracksPerCyl;			//written to vol
    USHORT	SectorsPerTrk;			//written to vol
    USHORT	BytesPerSec;			//written to vol
    WCHAR	DriveLetter[3];			//written to vol
    USHORT	nonloop;			//used on adding volume
    WCHAR	FilePath[MAX_PATH2];
} CREATE_VOL, *PCREATE_VOL;

#pragma pack()

typedef struct _VOL_STATS {
    ULONG	ReadCount;
    ULONG	WriteCount;
    WCHAR	DriveLetter[3];
} VOL_STATS, *PVOL_STATS;
