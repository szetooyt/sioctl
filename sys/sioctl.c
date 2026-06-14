/*++

Copyright (c) 1990-98  Microsoft Corporation All Rights Reserved

Module Name:

    sioctl.c

Abstract:

    Purpose of this driver is to demonstrate how the four different types
    of IOCTLs can be used, and how the I/O manager handles the user I/O
    buffers in each case. This sample also helps to understand the usage of
    some of the memory manager functions.

Environment:

    Kernel mode only.

--*/


//
// Include files.
//

#include <ntddk.h>          // various NT definitions
#include <string.h>
#include <mountmgr.h>		//szetoo : added for IOCTL_MOUNTMGR_QUERY_POINTS
#include <mountdev.h>		//szetoo : for IOCTL_MOUNTDEV_QUERY_UNIQUE_ID
#include <initguid.h>		//szetoo : GUID_DEVINTERFACE_DISK defined here
// #include <ntddstor.h>		//szetoo : GUID_DEVINTERFACE_DISK defined here

// #include <ntddft.h>		//szetoo : added for 
#include <ntdddisk.h>		//szetoo : added for DISK_GEOMETRY
#include <ntddvol.h>		//szetoo : for IOCTL_DISK_GET_PARTITION_INFO_EX
#include "ntstrsafe.h"		//szetoo : for RtlStringCchCopyW

#include "sioctl.h"
#include "debug.h"
//#include "sioctl.tmh"

// for dummy device name
#define NT_DEVICE_NAME      L"\\Device\\SIOCTL"
// creating symbolic link to dummy device. usermode open by using \\.\IoctlTest
#define DOS_DEVICE_NAME     L"\\DosDevices\\IoctlTest"
#define DOS_DEVNAME_LENGTH              (sizeof(DOS_DEVICE_NAME))
// for IOCTL_MOUNTDEV_QUERY_DEVICE_NAME
#define UNIQUE_VOLUME_ID	L"\\??\\Volume{e31129f0-e42e-11de-a569-806d6172696f}"
// for IOCTL_MOUNTDEV_QUERY_UNIQUE_ID
#define UNIQUE_ID		L"\\??\\StorageSzetoo"
// for IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME
#define LINK_NAME		L"\\DosDevices\\B:"

#if DBG
#define SIOCTL_KDPRINT(_x_) \
                DbgPrint("SIOCTL.SYS: ");\
                DbgPrint _x_;

#else
#define SIOCTL_KDPRINT(_x_)
#endif

//
// Device driver routine declarations.
//

DRIVER_INITIALIZE DriverEntry;

__drv_dispatchType(IRP_MJ_CREATE)
__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH SioctlCreateClose;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH SioctlDeviceControl;

DRIVER_UNLOAD SioctlUnloadDriver;


NTSTATUS
SioctlAddDevice( 
    IN PDEVICE_OBJECT DeviceObject,
    IN PCREATE_VOL	pCreateVol
    );
DRIVER_DISPATCH SioctlReadWrite;
//NTSTATUS
//SioctlReadWrite(
//    IN PDEVICE_OBJECT DeviceObject,
//    IN PIRP Irp
//    );

VOID
RamDiskCleanUp( 
    IN PDEVICE_OBJECT DeviceObject
    );




VOID
PrintIrpInfo(
    PIRP Irp
    );
VOID
PrintChars(
    __in_ecount(CountChars) PCHAR BufferAddress,
    __in size_t CountChars
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, SioctlCreateClose)
#pragma alloc_text( PAGE, SioctlDeviceControl)
#pragma alloc_text( PAGE, SioctlUnloadDriver)
#pragma alloc_text( PAGE, SioctlAddDevice)
#pragma alloc_text( PAGE, PrintIrpInfo)
#pragma alloc_text( PAGE, PrintChars)
#endif // ALLOC_PRAGMA


NTSTATUS
DriverEntry(
    __in PDRIVER_OBJECT   DriverObject,
    __in PUNICODE_STRING      RegistryPath
    )
/*++

Routine Description:
    This routine is called by the Operating System to initialize the driver.

    It creates the device object, fills in the dispatch entry points and
    completes the initialization.

Arguments:
    DriverObject - a pointer to the object that represents this device
    driver.

    RegistryPath - a pointer to our Services key in the registry.

Return Value:
    STATUS_SUCCESS if initialized; an error otherwise.

--*/

{
    NTSTATUS        ntStatus;
    UNICODE_STRING  ntUnicodeString;    // NT Device Name "\Device\SIOCTL"
    UNICODE_STRING  ntWin32NameString;    // Win32 Name "\DosDevices\IoctlTest"
    PDEVICE_OBJECT  deviceObject = NULL;    // ptr to device object
    PRAMDISK_DRIVER_EXTENSION   driverExtension;

    PDEVICE_EXTENSION           devExt;
    UNICODE_STRING              uniWin32Name;

    UNREFERENCED_PARAMETER(RegistryPath);

    //WPP_INIT_TRACING( DriverObject, RegistryPath );
    //DoTraceMessage(FLAG_ONE, "DriverEntry - IN.");


    //
    // Create extension for the driverobject to store driver specific 
    // information. Device specific information should be stored in
    // Device Extension

    ntStatus = IoAllocateDriverObjectExtension(DriverObject,
                                             RAMDISK_DRIVER_EXTENSION_KEY,
                                             sizeof(RAMDISK_DRIVER_EXTENSION),
                                             &driverExtension);

    if(!NT_SUCCESS(ntStatus)) {
         DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, 
            ("Ramdisk driver extension could not be allocated %lx \n", ntStatus ) );
        return ntStatus;
    }

    RtlInitUnicodeString( &ntUnicodeString, NT_DEVICE_NAME );
    //DoTraceMessage(FLAG_ONE, ""DriverEntry  init nt_device_name = %WZ", &(ntUnicodeString));

    ntStatus = IoCreateDevice(
        DriverObject,                   // Our Driver Object
        sizeof(DEVICE_EXTENSION),       // 0 We don't use a device extension
        &ntUnicodeString,               // Device name "\Device\SIOCTL"
        FILE_DEVICE_UNKNOWN,            // Device type
        FILE_DEVICE_SECURE_OPEN,     // Device characteristics
        FALSE,                          // Not an exclusive device
        &deviceObject );                // Returned ptr to Device Object

    if ( !NT_SUCCESS( ntStatus ) )
    {
        SIOCTL_KDPRINT(("Couldn't create the device object\n"));
        return ntStatus;
    }
    devExt = deviceObject->DeviceExtension;

    // when is dummy dev SymbolicLink reference ?
    // if needed it is better to put L"\\DosDevices\\IoctlTest"
    devExt->SymbolicLink.Buffer = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            DOS_DEVNAME_LENGTH,
                            RAMDISK_TAG_GENERAL);		//free when cleanup

    if ( devExt->SymbolicLink.Buffer == NULL ) {
	//DoTraceMessage(FLAG_ONE, "Can't allocate memory for symbolic link");
        //DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, ("Can't allocate memory for symbolic link\n") );
        
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlInitUnicodeString( &uniWin32Name, DOS_DEVICE_NAME );
    //DoTraceMessage(FLAG_ONE, "after init unicodestring = %wZ", &uniWin32Name);

    devExt->SymbolicLink.MaximumLength = DOS_DEVNAME_LENGTH;
    devExt->SymbolicLink.Length = uniWin32Name.Length;

    RtlCopyUnicodeString( &(devExt->SymbolicLink), &uniWin32Name );




    //DoTraceMessage(FLAG_ONE, "DriverEntry DriverObject= %p", DriverObject);
    //DoTraceMessage(FLAG_ONE, "DriverEntry d->devobj = %p", DriverObject->DeviceObject );
    //DoTraceMessage(FLAG_ONE, "DriverEntry    devobj = %p", deviceObject );
    //DriverObject->DeviceObject = deviceObject;
    //
    // Initialize the driver object with this driver's entry points.
    //
    DriverObject->MajorFunction[IRP_MJ_READ]           = SioctlReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE]          = SioctlReadWrite;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = SioctlCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = SioctlCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SioctlDeviceControl;
    DriverObject->DriverUnload = SioctlUnloadDriver;

    //
    // Initialize a Unicode String containing the Win32 name
    // for our device.
    //

    RtlInitUnicodeString( &ntWin32NameString, DOS_DEVICE_NAME ); //L"\\DosDevices\\IoctlTest"

    //
    // Create a symbolic link between our device name  and the Win32 name
    //

    ntStatus = IoCreateSymbolicLink(
                        &ntWin32NameString, &ntUnicodeString );

    if ( !NT_SUCCESS( ntStatus ) )
    {
        //
        // Delete everything that this routine has allocated.
        //
        SIOCTL_KDPRINT(("Couldn't create symbolic link\n"));
        IoDeleteDevice( deviceObject );
    }

    return ntStatus;
}


NTSTATUS
SioctlCreateClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
/*++

Routine Description:

    This routine is called by the I/O system when the SIOCTL is opened or
    closed.

    No action is performed other than completing the request successfully.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    NT status code

--*/

{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status = STATUS_SUCCESS;
    //UNICODE_STRING uniWin32NameString;
    //PDEVICE_EXTENSION   devExt;
    
    PDRIVER_OBJECT		DriverObject;
    //PDEVICE_OBJECT 		iterDeviceObject;
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    DriverObject = DeviceObject->DriverObject;

    DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_VERBOSE, ("CreateClose - IN\n") );

    irpStack = IoGetCurrentIrpStackLocation( Irp );
    //DoTraceMessage(FLAG_ONE, "RamDiskCreateClose requestormode = %d", Irp->RequestorMode); 
    switch ( irpStack->MajorFunction ) {

        case IRP_MJ_CREATE:
            DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_INFO, ("IRP_MJ_CREATE (%p)\n", Irp) );
	    //DoTraceMessage(FLAG_ONE, "RamDiskCreateClose create");
            COMPLETE_REQUEST( Irp, status, 0 );
            break;

        case IRP_MJ_CLOSE:
            DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_INFO, ("IRP_MJ_CLOSE (%p)\n", Irp) );
	    //DoTraceMessage(FLAG_ONE, "RamDiskCreateClose close");
/*
    RtlInitUnicodeString( &uniWin32NameString, DOS_DEVICE_NAME );
    //
    // Delete the link from our device name to a name in the Win32 namespace.
    //

    iterDeviceObject = DriverObject->DeviceObject;
    while (iterDeviceObject != NULL )
    {
	devExt = iterDeviceObject->DeviceExtension;
        
	//DoTraceMessage(FLAG_ONE, "SioctlCreateClose IoDeleteDevice");
	RamDiskCleanUp( iterDeviceObject );
	iterDeviceObject = iterDeviceObject->NextDevice;        
    }
*/
            COMPLETE_REQUEST( Irp, status, 0 );
            break;

        default:
            status = STATUS_NOT_IMPLEMENTED;
            COMPLETE_REQUEST( Irp, status, 0 );
            ASSERTMSG("BUG: we should never get here", 0);
            break;

    } // switch

    DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_VERBOSE, ("CreateClose - OUT\n") );

    return status;
}

VOID
RamDiskCleanUp( 
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine does the required cleaning like deleting the symbolic link
    releasjing the memory etc.

Arguments:

    DeviceObject - Supplies a pointer to the device object that represents
        the device whose capacity is to be read.

Return Value:

    None.

--*/
{
      
    PDEVICE_EXTENSION   devExt = DeviceObject->DeviceExtension;

    PAGED_CODE();
//DoTraceMessage(FLAG_ONE, "RamDiskCleanUp");
    DBGPRINT( DBG_COMP_PNP, DBG_LEVEL_VERBOSE, ("RamDiskCleanUp\n" ) );

    if ( devExt->Flags & FLAG_LINK_CREATED ) {
	//DoTraceMessage(FLAG_ONE, "device is = %wZ", &(devExt->SymbolicLink));
	IoSetDeviceInterfaceState (  &(devExt->SymbolicLink) , FALSE ) ;
        IoDeleteSymbolicLink( &(devExt->SymbolicLink) );
    }
    if ( devExt->SymbolicLink.Buffer ) {
	//DoTraceMessage(FLAG_ONE, "ExFreePool SymbolicLink.Buffer\n");
        ExFreePool( devExt->SymbolicLink.Buffer );
    }
//    if ( devExt->DiskRegInfo.DriveLetter.Buffer ) {
//	DoTraceMessage(FLAG_ONE, "ExFreePool DiskRegInfo.DriveLetter.Buffer\n");
//        ExFreePool( devExt->DiskRegInfo.DriveLetter.Buffer );
//    }
    if ( devExt->FileHandle ) { 
	//DoTraceMessage(FLAG_ONE, "free file handle\n");
        ZwClose(devExt->FileHandle);
    }
//    if ( devExt->LowerDeviceObject ) {
//        IoDetachDevice( devExt->LowerDeviceObject );
//    }
    //DoTraceMessage(FLAG_ONE, "RamDiskCleanUp reference count = %d\n", DeviceObject->ReferenceCount);
    IoDeleteDevice( DeviceObject );

    return;
}

NTSTATUS
SioctlReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called by the I/O system to read or write to a
    device that we control. It can also be called by
    RamDiskDispatchDeviceControl() to do a VERIFY.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    Status based on the request

--*/

{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    ULONG               information = 0;
    PUCHAR              currentAddress;
    //LARGE_INTEGER  		interval;
    IO_STATUS_BLOCK             ioStatus;
    FILE_POSITION_INFORMATION   position;
    UCHAR			mti;		//moveto table index
    UCHAR			keep;
    UCHAR			*ps;
    USHORT			i;
    PUCHAR			pBuffer;	//cant process currentAddress directly, try this

    PDEVICE_EXTENSION   devExt = DeviceObject->DeviceExtension;
    DBGPRINT( DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("ReadWrite - IN DevState: %x \n", devExt->DevState) );

    //dotraceMessage(FLAG_ONE, "ReadWrite reference count = %d\n", DeviceObject->ReferenceCount);
//interval.QuadPart = (LONGLONG) -5000000;
//KeDelayExecutionThread(KernelMode, FALSE, &interval );

    if ( devExt->DevState != WORKING ) {
        //
        // Device is not yet started or being removed, reject any IO request
        // TODO: Queue the IRPs
	//DoTraceMessage(FLAG_ONE, "Device not ready");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_READ, DBG_LEVEL_WARN, ("Device not ready\n" ) );
        status = STATUS_INVALID_DEVICE_STATE;
        COMPLETE_REQUEST( Irp, status, information );
    }
    status = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
	//DoTraceMessage(FLAG_ONE, "Acquire RemoveLock failed");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_PNP, DBG_LEVEL_ERROR, ("Acquire RemoveLock failed\n" ) );
        COMPLETE_REQUEST( Irp, status, 0 );
        return status;
    }

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // Check for invalid parameters.  It is an error for the starting offset
    // + length to go past the end of the buffer, or for the length to
    // not be a proper multiple of the sector size.
    //
    // Others are possible, but we don't check them since we trust the
    // file system
    //

//    if (RtlLargeIntegerGreaterThan(
//            RtlLargeIntegerAdd( 
//                irpStack->Parameters.Read.ByteOffset,
//                RtlConvertUlongToLargeInteger(irpStack->Parameters.Read.Length)),
//            devExt->DiskSize) ||
    if (
        (irpStack->Parameters.Read.Length & (devExt->BytesPerSec - 1)) ||
	(irpStack->Parameters.Read.ByteOffset.LowPart & (devExt->BytesPerSec - 1))) {
        //
        // Do not give an I/O boost for parameter errors.
        //
    //dotraceMessage(FLAG_ONE, "Error invalid parameter. Operation: %x", irpStack->MajorFunction);
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_READ, DBG_LEVEL_ERROR, 
            (
                "Error invalid parameter\n"
                "ByteOffset: %x\n"
                "Length: %d\n"
                "Operation: %x\n",
                irpStack->Parameters.Read.ByteOffset,
                irpStack->Parameters.Read.Length,
                irpStack->MajorFunction
            ));

        status = STATUS_INVALID_PARAMETER;
        COMPLETE_REQUEST( Irp, status, information );
        IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
        return status;
    }
    //dotraceMessage(FLAG_ONE, "b4 assert mdl address = %p", Irp->MdlAddress);

    //
    // Get a system-space pointer to the user's buffer.  A system
    // address must be used because we may already have left the
    // original caller's address space.
    //

    ASSERT ( Irp->MdlAddress != NULL );
    currentAddress = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

    //
    // The mapping request can fail if system is very low on resources.
    // Check for NULL and return approriate error status if the mapping failed
    //

    if ( currentAddress == NULL ) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        COMPLETE_REQUEST( Irp, status, information );
        IoReleaseRemoveLock(&devExt->RemoveLock, Irp);
	//DoTraceMessage(FLAG_ONE, "Unable to get the system-space virtual address");

        DBGPRINT( DBG_COMP_READ, DBG_LEVEL_ERROR, ("Unable to get the system-space virtual address\n" ) );
        return status;
    }
    //dotraceMessage(FLAG_ONE, "Irp of Request: %x Vmem Address of Transfer: %x - %x", (ULONG)Irp, (ULONG)currentAddress, ((ULONG)currentAddress) + irpStack->Parameters.Read.Length);
    //dotraceMessage(FLAG_ONE, "Length of Transfer: %d Operation: %x Starting ByteOffset: %x", irpStack->Parameters.Read.Length,irpStack->MajorFunction, irpStack->Parameters.Read.ByteOffset.LowPart);

    DBGPRINT( DBG_COMP_READ, DBG_LEVEL_VERBOSE,
        (
            "Irp of Request: %x\n"
            "Vmem Address of Transfer: %x - %x\n"
            "Length of Transfer: %d\n"
            "Operation: %x\n"
            "Starting ByteOffset: 0x%.8x%.8x\n",
            Irp,
            currentAddress,
            ((PUCHAR)currentAddress) + irpStack->Parameters.Read.Length,
            irpStack->Parameters.Read.Length,
            irpStack->MajorFunction,
            irpStack->Parameters.Read.ByteOffset.HighPart,irpStack->Parameters.Read.ByteOffset.LowPart
        ));

    information = irpStack->Parameters.Read.Length;

    switch (irpStack->MajorFunction) {

    case IRP_MJ_READ:
	//DoTraceMessage(FLAG_ONE, "IRP_MJ_READ");

	
        position.CurrentByteOffset.QuadPart = irpStack->Parameters.Read.ByteOffset.QuadPart + 512;
        status = ZwSetInformationFile(devExt->FileHandle,
                             &ioStatus,
                             &position,
                             sizeof(FILE_POSITION_INFORMATION),
                             FilePositionInformation);
        DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_READ ZwSetInformationFile byteOffset 0x%.8x%.8x status:%x\n",
            position.CurrentByteOffset.HighPart, position.CurrentByteOffset.LowPart, status));
        if (NT_SUCCESS(status)) {
        	pBuffer = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            irpStack->Parameters.Read.Length,
                            RAMDISK_TAG_GENERAL);
            if (pBuffer == NULL) {  //compiler complain this might be 0
                DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_READ ExAllocatePool2 Null\n"));
                break;
            }
            DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_READ b4 ZwReadFile filehandle:%p areAllApcsDisabled:%d IRQL:%X\n",devExt->FileHandle,
			 KeAreAllApcsDisabled(), KeGetCurrentIrql() ));
            status = ZwReadFile (devExt->FileHandle,
                                NULL,//   Event,
                                NULL,// PIO_APC_ROUTINE  ApcRoutine
                                NULL,// PVOID  ApcContext
                                &ioStatus,
                                pBuffer,		//prevly currentAddress,
                                (ULONG)irpStack->Parameters.Read.Length,
                                0, // ByteOffset
                                NULL // Key
                                );
            DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_READ after ZwReadFile status:%x\n", status));
//	    if (devExt->startPrint) {
//	    devExt->donePrint = TRUE;		//disable write gate
//	    devExt->startPrint = FALSE;		//disable this gate too
	        ps = pBuffer;				//prevly currentAddress;
	        while (ps - pBuffer < (LONG)irpStack->Parameters.Read.Length) {
// processing buffer..
	        }
	    
//	        } //startPrint
	        //copy to currentAddress
	        RtlCopyMemory(currentAddress, pBuffer, irpStack->Parameters.Read.Length);
	        devExt->ReadCount++;
            DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("first bytes %2X %2X %2X %2X %2X %2X %2X %2X %2X %2X \n", pBuffer[0],
                pBuffer[1], pBuffer[2], pBuffer[3], pBuffer[4], pBuffer[5], pBuffer[6], pBuffer[7], pBuffer[8], pBuffer[9]));
            if (pBuffer != NULL) {  //compiler complain pBuffer might be 0
	            ExFreePool( pBuffer );
            }
        }

        break;

    case IRP_MJ_WRITE:
	//DoTraceMessage(FLAG_ONE, "IRP_MJ_WRITE");
//	if (! devExt->donePrint ) {
//	devExt->startPrint = TRUE;
	
    	pBuffer = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            irpStack->Parameters.Read.Length,
                            RAMDISK_TAG_GENERAL);
        if (pBuffer == NULL) {  //compiler complain this might be 0
            DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_WRITE ExAllocatePool2 Null\n"));
            break;
        }
    	RtlCopyMemory(pBuffer, currentAddress, irpStack->Parameters.Read.Length);
	    ps = pBuffer;
    	while (ps - pBuffer < (LONG)irpStack->Parameters.Read.Length) {
// process buffer
	} 
        position.CurrentByteOffset.QuadPart = irpStack->Parameters.Read.ByteOffset.QuadPart + 512;

        status = ZwSetInformationFile(devExt->FileHandle,
                             &ioStatus,
                             &position,
                             sizeof(FILE_POSITION_INFORMATION),
                             FilePositionInformation);
        DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_WRITE ZwSetInformationFile status:%x\n", status));
        if (NT_SUCCESS(status))
        {

            status = ZwWriteFile(devExt->FileHandle,
                                NULL,//   Event,
                                NULL,// PIO_APC_ROUTINE  ApcRoutine
                                NULL,// PVOID  ApcContext
                                &ioStatus,
                                pBuffer,
                                (ULONG)irpStack->Parameters.Read.Length,
                                0, // ByteOffset
                                NULL // Key
                                );
            DBGPRINT(DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("IRP_MJ_WRITE ZwWriteFile status:%x\n", status));
	        devExt->WriteCount++;
        }
        if (pBuffer != NULL) {  //compiler complain this might be 0
    	    ExFreePool( pBuffer );
        }
        break;

    default:
        information = 0;
        break;
    }

    status = STATUS_SUCCESS;
    COMPLETE_REQUEST( Irp, status, information );
    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);

//DoTraceMessage(FLAG_ONE, "ReadWrite - OUT reference count = %d\n", DeviceObject->ReferenceCount);
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
    DBGPRINT( DBG_COMP_READ, DBG_LEVEL_VERBOSE, ("ReadWrite - OUT \n" ) );
    return status;
}   // End of RamDiskReadWrite()

VOID
SioctlUnloadDriver(
    __in PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    This routine is called by the I/O system to unload the driver.

    Any resources previously allocated must be freed.

Arguments:

    DriverObject - a pointer to the object that represents our driver.

Return Value:

    None
--*/

{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING uniWin32NameString;
    //LARGE_INTEGER  		interval;

    PAGED_CODE();

    //dotraceMessage(FLAG_ONE, "SioctlUnloadDriver d->devobj = %p", DriverObject->DeviceObject );
    //dotraceMessage(FLAG_ONE, "SioctlUnloadDriver    devobj = %p", deviceObject );

    //dotraceMessage(FLAG_ONE, "SioctlUnloadDriver - IN.");
//interval.QuadPart = (LONGLONG) -5000000;
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
    //
    // Create counted string version of our Win32 device name.
    //

    RtlInitUnicodeString( &uniWin32NameString, DOS_DEVICE_NAME );	//L"\\DosDevices\\IoctlTest"


    //
    // Delete the link from our device name to a name in the Win32 namespace.
    //

    IoDeleteSymbolicLink( &uniWin32NameString );

    if ( deviceObject != NULL )
    {
	//DoTraceMessage(FLAG_ONE, "SioctlUnloadDriver IoDeleteDevice");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        IoDeleteDevice( deviceObject );
    }

    //WPP_CLEANUP( DriverObject );


}

NTSTATUS
SioctlDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to perform a device I/O
    control function.

Arguments:

    DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    NT status code

--*/

{
    //PIO_STACK_LOCATION  irpSp;// Pointer to current stack location
    NTSTATUS            ntStatus = STATUS_SUCCESS;// Assume success
    ULONG               inBufLength; // Input buffer length
    ULONG               outBufLength; // Output buffer length
    PCHAR               inBuf; // pointer to Input and output buffer
    PCHAR		outBuf;
    //PCHAR               data = "This String is from Device Driver !!!";
    //size_t              datalen = strlen(data)+1;//Length of data including null
    //PMDL                mdl = NULL;
    //PCHAR               buffer = NULL;
    PIO_STACK_LOCATION  irpStack;
    ULONG               command;
    ULONG               information = 0;
    //LARGE_INTEGER  		interval;



    //UNREFERENCED_PARAMETER(DeviceObject);
    PDEVICE_EXTENSION    devExt = DeviceObject->DeviceExtension;

    PAGED_CODE();

    //dotraceMessage(FLAG_ONE, "SioctlDeviceControl - IN. reference count = %d\n", DeviceObject->ReferenceCount);


    //irpSp = IoGetCurrentIrpStackLocation( Irp );
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    command = irpStack->Parameters.DeviceIoControl.IoControlCode;

    if ( command != IOCTL_SIOCTL_METHOD_ADD_VOLUME && 
	command != IOCTL_SIOCTL_METHOD_UMOUNT &&
	command != IOCTL_SIOCTL_METHOD_STATS_COUNT &&
	command != IOCTL_SIOCTL_METHOD_BUFFERED &&
	command != IOCTL_SIOCTL_VOLUME_ARRIVAL) 
    {
//	inBufLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
//	outBufLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
//    } else {
	ntStatus = IoAcquireRemoveLock(&devExt->RemoveLock, Irp);
	if (!NT_SUCCESS(ntStatus)) {
	    //dotraceMessage(FLAG_ONE, "Acquire RemoveLock failed");
	    DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_ERROR, ("Acquire RemoveLock failed\n" ) );
	    //COMPLETE_REQUEST( Irp, status, 0 );
	    Irp->IoStatus.Status = ntStatus;
	    Irp->IoStatus.Information = information;
	    IoCompleteRequest( Irp, IO_NO_INCREMENT );

	    return ntStatus;
	}
    }
    else {
	inBufLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	outBufLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    }
//    if (!inBufLength || !outBufLength)
//    {
//        ntStatus = STATUS_INVALID_PARAMETER;
//        goto End;
//    }

    //
    // Determine which I/O control code was specified.
    //

    switch (  command  )
    {
    case IOCTL_SIOCTL_METHOD_ADD_VOLUME: {
    	PCREATE_VOL	pCreateVol;
	    UNICODE_STRING	test;

	   
        //
        // In this method the I/O manager allocates a buffer large enough to
        // to accommodate larger of the user input buffer and output buffer,
        // assigns the address to Irp->AssociatedIrp.SystemBuffer, and
        // copies the content of the user input buffer into this SystemBuffer
        //

        SIOCTL_KDPRINT(("Called IOCTL_SIOCTL_ADD_VOLUME\n"));
        PrintIrpInfo(Irp);

        //
        // Input buffer and output buffer is same in this case, read the
        // content of the buffer before writing to it
        //

        pCreateVol = (PCREATE_VOL)Irp->AssociatedIrp.SystemBuffer;

        //
        // Read the data from the buffer
        //
/*	DoTraceMessage(FLAG_ONE, "DiskSize=%I64u Cyl=%d Track=%d Sector=%d Byte/Sec=%d",
		pCreateVol->DiskSize.QuadPart,
		pCreateVol->Cylinders,
		pCreateVol->TracksPerCyl,
		pCreateVol->SectorsPerTrk,
        	pCreateVol->BytesPerSec);
*/	

    	RtlInitUnicodeString(&test, (PCWSTR)&pCreateVol->FilePath);
	//DoTraceMessage(FLAG_ONE, "l=%d m=%d filepath3=%wZ", test.Length, test.MaximumLength, &test);
	    RtlInitUnicodeString(&test, (PCWSTR)&pCreateVol->DriveLetter);
	//DoTraceMessage(FLAG_ONE, "l=%d m=%d DriveLetter=%wZ", test.Length, test.MaximumLength, &test);

        

        //
        // When the Irp is completed the content of the SystemBuffer
        // is copied to the User output buffer and the SystemBuffer is
        // is freed.
        //

    	// create disk device object
	    ntStatus = SioctlAddDevice( DeviceObject, pCreateVol );

    	information = 0;
	    Irp->IoStatus.Status = ntStatus;
    	Irp->IoStatus.Information = information;
	    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    	return ntStatus;
    }

    case IOCTL_SIOCTL_METHOD_UMOUNT: {
	    PCREATE_VOL	pCreateVol;
    	PDRIVER_OBJECT		DriverObject;
	    PDEVICE_OBJECT 	iterDeviceObject;
	    PDEVICE_EXTENSION    devExt2;
	    UNICODE_STRING	test;

        SIOCTL_KDPRINT(("Called IOCTL_SIOCTL_UMOUNT\n"));
        PrintIrpInfo(Irp);

    	pCreateVol = (PCREATE_VOL)Irp->AssociatedIrp.SystemBuffer;
/*	DoTraceMessage(FLAG_ONE, "Umount DiskSize=%I64u Cyl=%d Track=%d Sector=%d Byte/Sec=%d",
		pCreateVol->DiskSize.QuadPart,
		pCreateVol->Cylinders,
		pCreateVol->TracksPerCyl,
		pCreateVol->SectorsPerTrk,
        	pCreateVol->BytesPerSec);
*/
    	RtlInitUnicodeString(&test, (PCWSTR)&pCreateVol->DriveLetter);
	//DoTraceMessage(FLAG_ONE, "l=%d m=%d DriveLetter=%wZ", test.Length, test.MaximumLength, &test);

    	DriverObject = DeviceObject->DriverObject;
    	iterDeviceObject = DriverObject->DeviceObject;
	    while (iterDeviceObject != NULL ) {
	        devExt2 = iterDeviceObject->DeviceExtension;
	    //dotraceMessage(FLAG_ONE, "Count device %p", iterDeviceObject);
    	    if (RtlCompareMemory(pCreateVol->DriveLetter, devExt2->DriveLetter, 6) >= 4) {
	        	RamDiskCleanUp(iterDeviceObject);
		//DoTraceMessage(FLAG_ONE, "Delete SymbolicLink and device");

        		information = 0;
		        Irp->IoStatus.Status = ntStatus;
        		Irp->IoStatus.Information = information;
        		IoCompleteRequest( Irp, IO_NO_INCREMENT );
        		return STATUS_SUCCESS;
    	    }
    	    iterDeviceObject = iterDeviceObject->NextDevice;
        }
	    ntStatus = STATUS_OBJECT_PATH_NOT_FOUND;
    	Irp->IoStatus.Status = ntStatus;
	    Irp->IoStatus.Information = 0;
	    IoCompleteRequest( Irp, IO_NO_INCREMENT );
	    return ntStatus;

    }
    case IOCTL_SIOCTL_METHOD_STATS_COUNT: {
	    PDRIVER_OBJECT		DriverObject;
	    PDEVICE_OBJECT		iterDeviceObject;
	    PDEVICE_EXTENSION	devExt2;
	    PVOL_STATS		pVolStats;
	    USHORT			VolLimit;
	    USHORT			*pVolCount;
        //LARGE_INTEGER  		interval;		//delay traceview

	//outBufLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
	    pVolCount = Irp->AssociatedIrp.SystemBuffer;
	    VolLimit = *pVolCount;
        SIOCTL_KDPRINT(("Called IOCTL_SIOCTL_METHOD_STATS_COUNT\n"));
        SIOCTL_KDPRINT(("pVolCount = %p VolCount=%d\n",pVolCount, *pVolCount));

	//DoTraceMessage(FLAG_ONE, "pVolCount = %p VolCount=%d", pVolCount, *pVolCount);
	//    interval.QuadPart = (LONGLONG) -10000000;
    	pVolStats = (PVOL_STATS)(pVolCount + 1);
	    *pVolCount = 0;
	    DriverObject = DeviceObject->DriverObject;
	    iterDeviceObject = DriverObject->DeviceObject;
    	while (iterDeviceObject != NULL ) {	    
	        devExt2 = iterDeviceObject->DeviceExtension;
	    //DoTraceMessage(FLAG_ONE, "iterDev %p flags=%X", iterDeviceObject, devExt2->Flags);
	    //KeDelayExecutionThread(KernelMode, FALSE, &interval );
	        if ( devExt2->Flags & FLAG_VOL_DEVICE ) {
		//DoTraceMessage(FLAG_ONE, "pVolStats = %p", pVolStats);
		//KeDelayExecutionThread(KernelMode, FALSE, &interval );

		        pVolStats->ReadCount = devExt2->ReadCount;
		        pVolStats->WriteCount = devExt2->WriteCount;
		        RtlStringCchCopyW(pVolStats->DriveLetter,
				        sizeof(pVolStats->DriveLetter),
				        devExt2->DriveLetter);
		// filename ?
		//DoTraceMessage(FLAG_ONE, "after copy %c%c%c", pVolStats->DriveLetter[0], pVolStats->DriveLetter[1], pVolStats->DriveLetter[2]);
		//KeDelayExecutionThread(KernelMode, FALSE, &interval );
    		    *pVolCount = *pVolCount + 1;
		//DoTraceMessage(FLAG_ONE, "VolCount=%d VolLimit=%d", *pVolCount, VolLimit );
		        if ( *pVolCount < VolLimit ) {
		            //DoTraceMessage(FLAG_ONE, "advancing pVolstat");
		            pVolStats++;
		        }
	        }
	    
	    //DoTraceMessage(FLAG_ONE, "b4 nextdevice pVolStats=%p volcnt=%d", pVolStats, *pVolCount);
	    //KeDelayExecutionThread(KernelMode, FALSE, &interval );
	        iterDeviceObject = iterDeviceObject->NextDevice;
        }
    	information = VolLimit * sizeof(VOL_STATS);
//DoTraceMessage(FLAG_ONE, "information = %d", VolLimit * sizeof(VOL_STATS) );
//interval.QuadPart = (LONGLONG) -200000000;
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
	    Irp->IoStatus.Status = ntStatus;
	    Irp->IoStatus.Information = information;
	    IoCompleteRequest( Irp, IO_NO_INCREMENT );
	    return STATUS_SUCCESS;

    }

    case IOCTL_SIOCTL_METHOD_BUFFERED: {

        //
        // In this method the I/O manager allocates a buffer large enough to
        // to accommodate larger of the user input buffer and output buffer,
        // assigns the address to Irp->AssociatedIrp.SystemBuffer, and
        // copies the content of the user input buffer into this SystemBuffer
        //
        SIOCTL_KDPRINT(("Called IOCTL_SIOCTL_METHOD_BUFFERED\n"));
        PrintIrpInfo(Irp);

        //
        // Input buffer and output buffer is same in this case, read the
        // content of the buffer before writing to it
        //

        inBuf = Irp->AssociatedIrp.SystemBuffer;
        outBuf = Irp->AssociatedIrp.SystemBuffer;

        //
        // Read the data from the buffer
        //

        //SIOCTL_KDPRINT(("\tData from User :"));
        //
        // We are using the following function to print characters instead
        // DebugPrint with %s format because we string we get may or
        // may not be null terminated.
        //
        //PrintChars(inBuf, inBufLength);

        //
        // Write to the buffer over-writes the input buffer content
        //

     //   RtlCopyBytes(outBuf, data, outBufLength);
	//DoTraceMessage(FLAG_ONE, "outbuf=%s", &outBuf);

        //SIOCTL_KDPRINT(("\tData to User : "));
        //PrintChars(outBuf, datalen  );

        //
        // Assign the length of the data copied to IoStatus.Information
        // of the Irp and complete the Irp.
        //
	//DoTraceMessage(FLAG_ONE, "outBufLength=%d datalen=%d", outBufLength, datalen);
//        Irp->IoStatus.Information = (outBufLength<datalen?outBufLength:datalen);
	//DoTraceMessage(FLAG_ONE, "irp->IoStatus.Information=%d", Irp->IoStatus.Information);

        //
        // When the Irp is completed the content of the SystemBuffer
        // is copied to the User output buffer and the SystemBuffer is
        // is freed.
        //

//	information = 0;
	//Irp->IoStatus.Status = ntStatus;
//	Irp->IoStatus.Information = information;
    	IoCompleteRequest( Irp, IO_NO_INCREMENT );

	    return STATUS_SUCCESS;
    }
    case IOCTL_DISK_GET_PARTITION_INFO_EX: {
        //dotraceMessage(FLAG_ONE, "IOCTL_DISK_GET_PARTITION_INFO_EX");
        SIOCTL_KDPRINT(("Called IOCTL_DISK_GET_PARTITION_INFO_EX\n"));
        PrintIrpInfo(Irp);
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
/***	if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof( PARTITION_INFORMATION_EX)) {
            //dotraceMessage(FLAG_ONE, "Output buffer too small...");
	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("Output buffer too small... \n" ) );
            ntStatus = STATUS_BUFFER_TOO_SMALL;       // Inform the caller we need bigger buffer
            information = sizeof( PARTITION_INFORMATION_EX);
        }
	else {
	    PPARTITION_INFORMATION_EX outputBuffer;
	    outputBuffer = ( PPARTITION_INFORMATION_EX )Irp->AssociatedIrp.SystemBuffer;

	    outputBuffer->PartitionStyle = PARTITION_STYLE_MBR;
	    outputBuffer->StartingOffset = RtlConvertUlongToLargeInteger(0);
	    outputBuffer->PartitionLength = RtlConvertUlongToLargeInteger(devExt->DiskSize);
	    outputBuffer->PartitionNumber = 1;
	    outputBuffer->RewritePartition = 0;
	    outputBuffer->Mbr.PartitionType = 0;
	    outputBuffer->Mbr.BootIndicator = 0;
	    outputBuffer->Mbr.RecognizedPartition = 0;
	    outputBuffer->Mbr.HiddenSectors = 0;
            ntStatus = STATUS_SUCCESS;
            information = sizeof(PARTITION_INFORMATION_EX );

	} *************************************/
        break;	
    } 
    case IOCTL_DISK_GET_PARTITION_INFO: {

        //dotraceMessage(FLAG_ONE, "IOCTL_DISK_GET_PARTITION_INFO");
        SIOCTL_KDPRINT(("Called IOCTL_DISK_GET_PARTITION_INFO printed by sioctl_kdprint()\n"));
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_GET_PARTITION_INFO \n" ) );
        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(PARTITION_INFORMATION)) {
            //dotraceMessage(FLAG_ONE, "Output buffer too small...");
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("Output buffer too small... \n" ) );
            ntStatus = STATUS_BUFFER_TOO_SMALL;       // Inform the caller we need bigger buffer
            information = sizeof(PARTITION_INFORMATION);
        } else {
            PPARTITION_INFORMATION outputBuffer;

            outputBuffer = ( PPARTITION_INFORMATION )Irp->AssociatedIrp.SystemBuffer;
	    //this code is not tested.
            outputBuffer->PartitionType = PARTITION_FAT32;

            outputBuffer->BootIndicator       = FALSE;
            outputBuffer->RecognizedPartition = TRUE;
            outputBuffer->RewritePartition    = FALSE;
            //outputBuffer->StartingOffset      = RtlConvertUlongToLargeInteger(0);
            outputBuffer->StartingOffset.HighPart = 0;
            outputBuffer->StartingOffset.LowPart = 0;
            outputBuffer->PartitionLength     = devExt->DiskSize;
            outputBuffer->HiddenSectors       = (ULONG) (1L);
            outputBuffer->PartitionNumber     = (ULONG) (-1L);



            ntStatus = STATUS_SUCCESS;
            information = sizeof( PARTITION_INFORMATION );
        }

        break;
    }
//subsequent media type is incorrect !
    case IOCTL_DISK_GET_MEDIA_TYPES: {
    	SIOCTL_KDPRINT(("Called IOCTL_DISK_GET_MEDIA_TYPES printed by sioctl_kdprint()\n"));
        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof( DISK_GEOMETRY ) * RemovableMedia )
        {
            //
            // Instead of returning STATUS_INVALID_PARAMETER, we will return
            // STATUS_BUFFER_TOO_SMALL and the required buffer size. 
            // So that the called will send a bigger buffer
            //
            ntStatus = STATUS_BUFFER_TOO_SMALL;       // Inform the caller we need bigger buffer
            information = sizeof( DISK_GEOMETRY ) * ( RemovableMedia + 1 ) ;
            //dotraceMessage(FLAG_ONE, "IOCTL_DISK_GET_MEDIA_TYPES - buffer too small");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_GET_MEDIA_TYPES - buffer too small\n" ) );
        }
        else
        {
            PDISK_GEOMETRY outputBuffer;

            outputBuffer = ( PDISK_GEOMETRY ) ( (ULONGLONG)Irp->AssociatedIrp.SystemBuffer + sizeof(DISK_GEOMETRY) * ( RemovableMedia - 1 ) ) ;
	    outputBuffer->Cylinders.QuadPart = devExt->Cylinders;
	    outputBuffer->MediaType = RemovableMedia;
	    outputBuffer->TracksPerCylinder = devExt->TracksPerCyl;
	    outputBuffer->SectorsPerTrack = devExt->SectorsPerTrk;
	    outputBuffer->BytesPerSector = devExt->BytesPerSec;

            ntStatus = STATUS_SUCCESS;
            information = sizeof( DISK_GEOMETRY ) * ( RemovableMedia + 1 ) ;
            //dotraceMessage(FLAG_ONE, "IOCTL_DISK_GET_MEDIA_TYPES - OK !");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_GET_MEDIA_TYPES - OK !\n" ) );
        }
        break;
    }

    case IOCTL_DISK_GET_DRIVE_GEOMETRY: {
	    SIOCTL_KDPRINT(("Called IOCTL_DISK_GET_DRIVE_GEOMETRY (%d) outputbufferLength:%d\n", sizeof(DISK_GEOMETRY), irpStack->Parameters.DeviceIoControl.OutputBufferLength));
        //
        // Return the drive geometry for the ram disk. Note that
        // we return values which were made up to suit the disk size.
        //

        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof( DISK_GEOMETRY ) )
        {
            //
            // Instead of returning STATUS_INVALID_PARAMETER, we will return
            // STATUS_BUFFER_TOO_SMALL and the required buffer size. 
            // So that the called will send a bigger buffer
            //
            ntStatus = STATUS_BUFFER_TOO_SMALL;       // Inform the caller we need bigger buffer
            //dotraceMessage(FLAG_ONE, "IOCTL_DISK_GET_DRIVE_GEOMETRY - buffer too small");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_GET_DRIVE_GEOMETRY - buffer too small\n" ) );
            information = sizeof(DISK_GEOMETRY);
        }
        else
        {
            PDISK_GEOMETRY outputBuffer;

            outputBuffer = ( PDISK_GEOMETRY ) Irp->AssociatedIrp.SystemBuffer;
            //RtlCopyMemory( outputBuffer, &(devExt->DiskGeometry), sizeof(DISK_GEOMETRY) );
	        outputBuffer->Cylinders.QuadPart = devExt->Cylinders;
	        outputBuffer->MediaType = RemovableMedia;
	        outputBuffer->TracksPerCylinder = devExt->TracksPerCyl;
	        outputBuffer->SectorsPerTrack = devExt->SectorsPerTrk;
	        outputBuffer->BytesPerSector = devExt->BytesPerSec;
            ntStatus = STATUS_SUCCESS;
	    //dotraceMessage(FLAG_ONE, "IOCTL_DISK_GET_DRIVE_GEOMETRY - OK !");
	    //KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_GET_DRIVE_GEOMETRY - OK ! \n"
                "Cylinder 0x%.8x%.8x\n"
                "Mediatype %d\n"
                "TracksPerCylinder  %u\n"
                "SectorsPerTrack = %u\n"
                "BytesPerSector= %u\n",
                outputBuffer->Cylinders.HighPart, outputBuffer->Cylinders.LowPart,
                outputBuffer->MediaType,
                outputBuffer->TracksPerCylinder,
                outputBuffer->SectorsPerTrack,
                outputBuffer->BytesPerSector ) );
            information = sizeof( DISK_GEOMETRY );

	    
        }
        break;
    }
    case IOCTL_STORAGE_CHECK_VERIFY:
    case IOCTL_DISK_CHECK_VERIFY: {
        //dotraceMessage(FLAG_ONE, "IOCTL_DISK_CHECK_VERIFY outputbuffer length=%d",irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_CHECK_VERIFY \n" ) );
	    if (irpStack->Parameters.DeviceIoControl.OutputBufferLength == 4) {
	       PULONG	pulong;
	       pulong = Irp->AssociatedIrp.SystemBuffer;
	       *pulong = 0;
	       information = 4;
	    }
	    else {
	       information = 0;
	    }
        //
        // Return status success
        //
        ntStatus = STATUS_SUCCESS;
        break;
    }                                        

    case IOCTL_DISK_IS_WRITABLE: {
	//DoTraceMessage(FLAG_ONE, "IOCTL_DISK_IS_WRITABLE");
	//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_IS_WRITABLE \n" ) );
        //
        // Return status success
        //
	information = 0;
        ntStatus = STATUS_SUCCESS;
        break;
    }                                        

    case IOCTL_DISK_SET_PARTITION_INFO: {
	    SIOCTL_KDPRINT(("Called IOCTL_DISK_SET_PARTITION_INFO printed by sioctl_kdprint()\n"));
	//DoTraceMessage(FLAG_ONE, "IOCTL_DISK_SET_PARTITION_INFO");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_SET_PARTITION_INFO\n" ) );
        // cannot handle this ....
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    case IOCTL_MOUNTMGR_QUERY_POINTS: {
        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTMGR_QUERY_POINTS");
    	SIOCTL_KDPRINT(("Called IOCTL_MOUNTMGR_QUERY_POINTS printed by sioctl_kdprint()\n"));
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_MOUNTMGR_QUERY_POINTS\n" ) );
        // cannot handle this ....
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;
       }

    case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME: {
	    SIOCTL_KDPRINT(("Called IOCTL_MOUNTDEV_QUERY_DEVICE_NAME outputbufferLength:%d\n", irpStack->Parameters.DeviceIoControl.OutputBufferLength));
        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(UNIQUE_VOLUME_ID) + sizeof(MOUNTDEV_NAME) )
        {
	        PMOUNTDEV_NAME outputBuffer;
            //
            // Instead of returning STATUS_INVALID_PARAMETER, we will return
            // STATUS_BUFFER_TOO_SMALL and the required buffer size. 
            // So that the called will send a bigger buffer
            //
            ntStatus = STATUS_BUFFER_OVERFLOW;       // Inform the caller we need bigger buffer
            information = sizeof(MOUNTDEV_NAME);
	    //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - buffer too small %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	    //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	    //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - device pointer %p", irpStack->DeviceObject);
	        outputBuffer = ( PMOUNTDEV_NAME ) Irp->AssociatedIrp.SystemBuffer;
	        outputBuffer->NameLength = sizeof(UNIQUE_VOLUME_ID) ;

//KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - buffer too small\n" ) );
        }
        else
        {
            PMOUNTDEV_NAME outputBuffer;
// "\Device\HarddiskVolume1", or
// "\DosDevices\D:", or a mount point such as
// "\DosDevices\E:\FilesysD\mnt". 

            outputBuffer = ( PMOUNTDEV_NAME ) Irp->AssociatedIrp.SystemBuffer;
//            outputBuffer->NameLength = sizeof(UNIQUE_VOLUME_ID);	// -2 ? why not NT_DEVICE_NAME2
//            RtlCopyMemory( &outputBuffer->Name[0] , UNIQUE_VOLUME_ID , outputBuffer->NameLength + sizeof(WCHAR) );
            outputBuffer->NameLength = sizeof(L"\\Device\\Ramdisk1");
            RtlCopyMemory(&outputBuffer->Name[0], L"\\Device\\Ramdisk1", outputBuffer->NameLength + sizeof(WCHAR));
            ntStatus = STATUS_SUCCESS;
            information = sizeof(L"\\Device\\Ramdisk1") + sizeof(MOUNTDEV_NAME) ;
//DoTraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - OK !");
	    //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - output buffer %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	    //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	    //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - namelength = %d", outputBuffer->NameLength);
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
            DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_MOUNTDEV_QUERY_DEVICE_NAME - OK ! name:%C %C %C %C %C %C %C %C %C %C %C %C %C %C %C %C\n",
                outputBuffer->Name[0], outputBuffer->Name[1], outputBuffer->Name[2], outputBuffer->Name[3], outputBuffer->Name[4], outputBuffer->Name[5],
                outputBuffer->Name[6], outputBuffer->Name[7], outputBuffer->Name[8], outputBuffer->Name[9], outputBuffer->Name[10], outputBuffer->Name[11],
                outputBuffer->Name[12], outputBuffer->Name[13], outputBuffer->Name[14], outputBuffer->Name[15]));

        }
        break;
    }
    case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID: {
    	SIOCTL_KDPRINT(("Called IOCTL_MOUNTDEV_QUERY_UNIQUE_ID outputBufferLength:%d\n", irpStack->Parameters.DeviceIoControl.OutputBufferLength));
	    if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(UNIQUE_ID) + sizeof(MOUNTDEV_UNIQUE_ID) )
            {
	        PMOUNTDEV_UNIQUE_ID outputBuffer;
                ntStatus = STATUS_BUFFER_OVERFLOW;       // Inform the caller we need bigger buffer
                information = sizeof(UNIQUE_ID) + sizeof(MOUNTDEV_UNIQUE_ID);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - buffer too small %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - device pointer %p", irpStack->DeviceObject);
	        outputBuffer = ( PMOUNTDEV_UNIQUE_ID ) Irp->AssociatedIrp.SystemBuffer;
	        outputBuffer->UniqueIdLength = sizeof(UNIQUE_ID) ;
    //	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
            }
	    else {
                PMOUNTDEV_UNIQUE_ID outputBuffer;

                outputBuffer = ( PMOUNTDEV_UNIQUE_ID ) Irp->AssociatedIrp.SystemBuffer;
                outputBuffer->UniqueIdLength = sizeof(UNIQUE_ID);	// -2 ?
                RtlCopyMemory( &outputBuffer->UniqueId[0] , UNIQUE_ID , outputBuffer->UniqueIdLength + sizeof(WCHAR) );
                ntStatus = STATUS_SUCCESS;
                information = sizeof(UNIQUE_ID) + sizeof(MOUNTDEV_UNIQUE_ID) ;
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - OK !");
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - output buffer %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_UNIQUE_ID - namelength = %d", outputBuffer->UniqueIdLength);
    //	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
	    }
	    break;
    }
    case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME: {	//L"\\DosDevices\\B:"
    	SIOCTL_KDPRINT(("Called IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME printed by sioctl_kdprint()\n"));
	    if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(LINK_NAME) + sizeof(MOUNTDEV_SUGGESTED_LINK_NAME) )
            {
	        PMOUNTDEV_SUGGESTED_LINK_NAME outputBuffer;
                ntStatus = STATUS_BUFFER_OVERFLOW;       // Inform the caller we need bigger buffer
                information = sizeof(MOUNTDEV_SUGGESTED_LINK_NAME);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - buffer too small %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - device pointer %p", irpStack->DeviceObject);
	        outputBuffer = ( PMOUNTDEV_SUGGESTED_LINK_NAME ) Irp->AssociatedIrp.SystemBuffer;
	        outputBuffer->NameLength = sizeof(LINK_NAME) ;
    //	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
            }
	    else {
                PMOUNTDEV_SUGGESTED_LINK_NAME outputBuffer;
	        USHORT i;

                outputBuffer = ( PMOUNTDEV_SUGGESTED_LINK_NAME ) Irp->AssociatedIrp.SystemBuffer;
	        outputBuffer->UseOnlyIfThereAreNoOtherLinks = TRUE;
                outputBuffer->NameLength = sizeof(LINK_NAME) -2;	// -2 ?
                RtlCopyMemory( &outputBuffer->Name[0] , LINK_NAME , outputBuffer->NameLength + 2+ sizeof(WCHAR) );
                ntStatus = STATUS_SUCCESS;
                information = sizeof(LINK_NAME) + sizeof(MOUNTDEV_SUGGESTED_LINK_NAME) ;
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - OK !");
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - output buffer %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	        //dotraceMessage(FLAG_ONE, "IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME - namelength = %d", outputBuffer->NameLength);
	        for (i=0; i < irpStack->Parameters.DeviceIoControl.OutputBufferLength; i++) {
			        /*DoTraceMessage(FLAG_ONE, "%d %c %d", i, *(((PUCHAR)outputBuffer) + i),
						    *(((PUCHAR)outputBuffer)+i)); */
	        }
    //	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
	    }
    	break;
    }
    case IOCTL_DISK_GET_LENGTH_INFO: {
    	SIOCTL_KDPRINT(("Called IOCTL_DISK_GET_LENGTH_INFO\n"));
	    if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_LENGTH_INFORMATION) )
            {
	        PGET_LENGTH_INFORMATION outputBuffer;
                ntStatus =  STATUS_BUFFER_TOO_SMALL;       // Inform the caller we need bigger buffer
                information = sizeof(GET_LENGTH_INFORMATION);
	        //DoTraceMessage(FLAG_ONE, "IOCTL_DISK_GET_LENGTH_INFO - buffer too small %d", irpStack->Parameters.DeviceIoControl.OutputBufferLength);
	        //DoTraceMessage(FLAG_ONE, "IOCTL_DISK_GET_LENGTH_INFO - input buffer %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	        outputBuffer = ( PGET_LENGTH_INFORMATION ) Irp->AssociatedIrp.SystemBuffer;
	        //outputBuffer->Length.QuadPart = sizeof(GET_LENGTH_INFORMATION);
    //	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
            }
	    else {
                PGET_LENGTH_INFORMATION outputBuffer;

                outputBuffer = ( PGET_LENGTH_INFORMATION) Irp->AssociatedIrp.SystemBuffer;
	        outputBuffer->Length = devExt->DiskSize;
                ntStatus = STATUS_SUCCESS;
                information = sizeof(GET_LENGTH_INFORMATION);
	        //DoTraceMessage(FLAG_ONE, "IOCTL_DISK_GET_LENGTH_INFO - OK !");
    //	    KeDelayExecutionThread(KernelMode, FALSE, &interval );
	    }

    	break;
    }
    case IOCTL_DISK_GET_DRIVE_LAYOUT_EX: {
    	SIOCTL_KDPRINT(("Called IOCTL_DISK_GET_DRIVE_LAYOUT_EX\n"));
	    if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 3*sizeof(PARTITION_INFORMATION_EX) )
            {
	       ntStatus = STATUS_INFO_LENGTH_MISMATCH;	// or STATUS_INSUFFICIENT_RESOURCES, or STATUS_BUFFER_TOO_SMALL.
	       information = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 3*sizeof(PARTITION_INFORMATION_EX);
	    }
	    else {
	       int i;
	       PDRIVE_LAYOUT_INFORMATION_EX		pDriveLIx;
	       pDriveLIx = (PDRIVE_LAYOUT_INFORMATION_EX) Irp->AssociatedIrp.SystemBuffer;
	       pDriveLIx->PartitionStyle = PARTITION_STYLE_MBR;
	       pDriveLIx->PartitionCount = 4;
	       pDriveLIx->Mbr.Signature= 0xB72A4F6;
	       pDriveLIx->PartitionEntry[0].PartitionStyle = PARTITION_STYLE_MBR;
	       pDriveLIx->PartitionEntry[0].StartingOffset.QuadPart = 125440;
	       pDriveLIx->PartitionEntry[0].PartitionLength.QuadPart = 1023809024;
	       pDriveLIx->PartitionEntry[0].PartitionNumber = 1;
	       pDriveLIx->PartitionEntry[0].RewritePartition = FALSE;
	       pDriveLIx->PartitionEntry[0].Mbr.PartitionType = PARTITION_HUGE;
	       pDriveLIx->PartitionEntry[0].Mbr.BootIndicator = FALSE;
	       pDriveLIx->PartitionEntry[0].Mbr.RecognizedPartition = TRUE;
	       pDriveLIx->PartitionEntry[0].Mbr.HiddenSectors = 245;
	       for (i=1; i < 4; i++) {
		       pDriveLIx->PartitionEntry[i].PartitionStyle = 0;
		       pDriveLIx->PartitionEntry[i].StartingOffset.QuadPart = 0;
		       pDriveLIx->PartitionEntry[i].PartitionLength.QuadPart = 0;
		       pDriveLIx->PartitionEntry[i].PartitionNumber = 0;
		       pDriveLIx->PartitionEntry[i].RewritePartition = FALSE;
		       pDriveLIx->PartitionEntry[i].Mbr.PartitionType = PARTITION_ENTRY_UNUSED;
		       pDriveLIx->PartitionEntry[i].Mbr.BootIndicator = FALSE;
		       pDriveLIx->PartitionEntry[i].Mbr.RecognizedPartition = FALSE;
		       pDriveLIx->PartitionEntry[i].Mbr.HiddenSectors = 0;
	       }
	       information = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 3*sizeof(PARTITION_INFORMATION_EX);
	       ntStatus = STATUS_SUCCESS;
	    }
	    break;
    }
    case IOCTL_DISK_MEDIA_REMOVAL: {
	// not implementing lock count
    	PPREVENT_MEDIA_REMOVAL inputBuffer;
    	SIOCTL_KDPRINT(("Called IOCTL_DISK_MEDIA_REMOVAL\n"));
    	inputBuffer = ( PPREVENT_MEDIA_REMOVAL) Irp->AssociatedIrp.SystemBuffer;
	    if ( inputBuffer->PreventMediaRemoval ) {
	        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
	    }
	    else {
	        ntStatus = STATUS_SUCCESS;
	    }
	    information = 0;
	//DoTraceMessage(FLAG_ONE, "IOCTL_DISK_MEDIA_REMOVAL %d", inputBuffer->PreventMediaRemoval);
	//DoTraceMessage(FLAG_ONE, "input buffer length %d", irpStack->Parameters.DeviceIoControl.InputBufferLength);
	//KeDelayExecutionThread(KernelMode, FALSE, &interval );
	    break;
    }
    case IOCTL_STORAGE_GET_HOTPLUG_INFO: {
        //DoTraceMessage(FLAG_ONE, "IOCTL_STORAGE_GET_HOTPLUG_INFO");
    	SIOCTL_KDPRINT(("Called IOCTL_STORAGE_GET_HOTPLUG_INFO\n"));
	    if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(STORAGE_HOTPLUG_INFO) )
            {
	       information = sizeof(STORAGE_HOTPLUG_INFO);
	       ntStatus = STATUS_BUFFER_TOO_SMALL ;
	    }
	    else {
	       PSTORAGE_HOTPLUG_INFO	outputbuffer;
	       outputbuffer = ( PSTORAGE_HOTPLUG_INFO ) Irp->AssociatedIrp.SystemBuffer;
	       //DoTraceMessage(FLAG_ONE, "IOCTL_STORAGE_GET_HOTPLUG_INFO");
	       outputbuffer->Size = 8; 	//sizeof(STORAGE_HOTPLUG_INFO)
	       outputbuffer->MediaRemovable = 1;
	       outputbuffer->MediaHotplug = 0;
	       outputbuffer->DeviceHotplug = 0;
	       outputbuffer->WriteCacheEnableOverride = 0;
	       ntStatus = STATUS_SUCCESS ;
	       information = sizeof(STORAGE_HOTPLUG_INFO);
	    }
        break;
//Size=8 MediaRemovable=1 MediaHotplug=0 DeviceHotplug=1 WriteCacheEnableOverride=0
    }
    case IOCTL_STORAGE_GET_DEVICE_NUMBER: {
	    SIOCTL_KDPRINT(("Called IOCTL_STORAGE_GET_DEVICE_NUMBER printed by sioctl_kdprint()\n"));
	    if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(STORAGE_DEVICE_NUMBER) )
            {
	       information = sizeof(STORAGE_HOTPLUG_INFO);
	       ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	    }
	    else {
	       PSTORAGE_DEVICE_NUMBER	outputbuffer;
               //DoTraceMessage(FLAG_ONE, "IOCTL_STORAGE_GET_DEVICE_NUMBER");
	       outputbuffer = ( PSTORAGE_DEVICE_NUMBER ) Irp->AssociatedIrp.SystemBuffer;
	       outputbuffer->DeviceType = 7 ;
	       outputbuffer->DeviceNumber = 1;
	       outputbuffer->PartitionNumber = 0;
	       ntStatus = STATUS_SUCCESS ;
	       information = sizeof(STORAGE_DEVICE_NUMBER);
	    }
	    break;
    }
/*
    case FT_BALANCED_READ_MODE: {
DoTraceMessage(FLAG_ONE, "FT_BALANCED_READ_MODE");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("FT_BALANCED_READ_MODE\n" ) );
        // cannot handle this ....
        ntStatus = STATUS_SUCCESS ;
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }
*/
    case IOCTL_VOLUME_ONLINE: {
	//DoTraceMessage(FLAG_ONE, "IOCTL_VOLUME_ONLINE");
    	SIOCTL_KDPRINT(("Called IOCTL_VOLUME_ONLINE printed by sioctl_kdprint()\n"));
//	KeDelayExecutionThread(KernelMode, FALSE, &interval );
        ntStatus = STATUS_SUCCESS ;
	    break;
    }
    case IOCTL_DISK_FORMAT_TRACKS: {
//DoTraceMessage(FLAG_ONE, "IOCTL_DISK_FORMAT_TRACKS");
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("IOCTL_DISK_FORMAT_TRACKS\n" ) );
        // cannot handle this ....
        ntStatus = STATUS_SUCCESS ;
        break;
    }
    case IOCTL_VOLUME_GET_GPT_ATTRIBUTES: {
        //DoTraceMessage(FLAG_ONE, "IOCTL_VOLUME_GET_GPT_ATTRIBUTES");
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;	
    }
    default: {
        //
        // Received not supported IOCTLs, fail them
        //DoTraceMessage(FLAG_ONE, "Unknown IOCTL command %X device x%X", command, DEVICE_TYPE_FROM_CTL_CODE(command));
	//DoTraceMessage(FLAG_ONE, "access %d function %d method %d", ACCESS_FROM_CTL_CODE(command),FUNCTION_FROM_CTL_CODE(command),METHOD_FROM_CTL_CODE(command));
	//KeDelayExecutionThread(KernelMode, FALSE, &interval );
        DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_INFO, ("Unknown IOCTL command %X\n", command ) );
        ntStatus = STATUS_NOT_SUPPORTED;
        break;
    }
    }  // end switch


    //
    // Finish the I/O operation by simply completing the packet and returning
    // the same status as in the packet itself.
    //

    IoReleaseRemoveLock(&devExt->RemoveLock, Irp);

    Irp->IoStatus.Status = ntStatus;
    Irp->IoStatus.Information = information;
    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    //DoTraceMessage(FLAG_ONE, "IOCtl- OUT reference count = %d\n", DeviceObject->ReferenceCount);
//KeDelayExecutionThread(KernelMode, FALSE, &interval );
    DBGPRINT( DBG_COMP_IOCTL, DBG_LEVEL_VERBOSE, ("IOCtl- OUT \n" ) );
    return ntStatus;
}


NTSTATUS
SioctlAddDevice( 
    IN PDEVICE_OBJECT DeviceObject,
    IN PCREATE_VOL	pCreateVol
    )
/*++ 
Routine Description:

    AddDevice routine to create the device object and symbolic link
    
Arguments:

    DriverObject            - Supplies the driver object

    PhysicalDeviceObject    - Supplies the physical device object
    
Return Value:

    NTSTATUS
    
--*/    
{

    PRAMDISK_DRIVER_EXTENSION   driverExtension;
    PDEVICE_OBJECT              functionDeviceObject;
    PDEVICE_EXTENSION           devExt;
    UNICODE_STRING              uniDeviceName;
    UNICODE_STRING              uniWin32Name;
    NTSTATUS                    status = STATUS_SUCCESS;
    PDRIVER_OBJECT		DriverObject;
    //PFILE_OBJECT  		PFileObject;
    //PDEVICE_OBJECT		PDeviceObject;
    OBJECT_ATTRIBUTES           fileAttributes;
    UNICODE_STRING              absFileName;
    IO_STATUS_BLOCK             ioStatus;
    //LARGE_INTEGER  		interval;
    //PMOUNTMGR_CREATE_POINT_INPUT	createPoint;
    //ULONG			createPointSize;
    //UNICODE_STRING		mntMgrname;
    //PFILE_OBJECT		pFileObject2;
    //PDEVICE_OBJECT		pDeviceObject2;
    //KEVENT			event;
    //PIRP			pirp;
    PDEVICE_OBJECT 		iterDeviceObject;
    ULONG			devCount = 0;
    UNICODE_STRING		uniDevCount;
    //LARGE_INTEGER  		interval;		//delay traceview

    PAGED_CODE();

    // get DriverObject first
    DriverObject = DeviceObject->DriverObject;


    //DoTraceMessage(FLAG_ONE, "AddDevice - IN. DriverObject=(%p) ",DriverObject);
    DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_VERBOSE, ("AddDevice - IN. DriverObject=(%p) \n",
        DriverObject ) );

    // Get the Driver object extension 


    driverExtension = IoGetDriverObjectExtension(DriverObject,
                                                 RAMDISK_DRIVER_EXTENSION_KEY);

//DoTraceMessage(FLAG_ONE, "driverExtension = %p", driverExtension);

//DoTraceMessage(FLAG_ONE, "Driverextension registrypath= %wZ",&(driverExtension->RegistryPath));


    ASSERT ( driverExtension != NULL );
//DoTraceMessage(FLAG_ONE, "after assert driverextension is not null");
	//
	// We are capable of handling only one device. If the we get AddDevice request 
	// for the next device, reject it
	//
	/***************** we are multi devices. how about multi open on same volume ?
	if ( driverExtension->DeviceInitialized == TRUE ) {
	    //DoTraceMessage(FLAG_ONE, "Device exists");
            //DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, ("Device exists\n") );
	    	return STATUS_DEVICE_ALREADY_ATTACHED;
	}
	******************/

//DoTraceMessage(FLAG_ONE, "before init nt_device name2");

    //
    // Create counted string version of our device name.
    //

//    RtlInitUnicodeString( &uniDeviceName, NT_DEVICE_NAME2 );	//L"\\Device\\Ramdisk"


    //count number of device created, to create unique uniDeviceName
    iterDeviceObject = DriverObject->DeviceObject;
    while (iterDeviceObject != NULL ) {
    	devCount++;
	//DoTraceMessage(FLAG_ONE, "Count device %p", iterDeviceObject);
//	interval.QuadPart = (LONGLONG) -10000000;
//	KeDelayExecutionThread(KernelMode, FALSE, &interval );
	    iterDeviceObject = iterDeviceObject->NextDevice;        
    }
    //RtlInitUnicodeString(&uniDevCount, L"00");
    uniDevCount.Buffer = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            4,
                            RAMDISK_TAG_GENERAL);
    uniDevCount.Length = 0;
    uniDevCount.MaximumLength = 4;

    //DoTraceMessage(FLAG_ONE, "unidevcount init to %wZ", &uniDevCount);
//    interval.QuadPart = (LONGLONG) -10000000;
//    KeDelayExecutionThread(KernelMode, FALSE, &interval );
    RtlIntegerToUnicodeString(devCount, 10, &uniDevCount);
    //DoTraceMessage(FLAG_ONE, "unidevcount = %wZ", &uniDevCount);
//    KeDelayExecutionThread(KernelMode, FALSE, &interval );
    //DoTraceMessage(FLAG_ONE, "uniDevCount = %wZ len=%d max=%d", &uniDevCount, uniDevCount.Length, uniDevCount.MaximumLength);
//    KeDelayExecutionThread(KernelMode, FALSE, &interval );
    uniDeviceName.Buffer = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            sizeof(NT_DEVICE_NAME2) + 3 * sizeof(WCHAR),
                            RAMDISK_TAG_GENERAL);
    uniDeviceName.Length = 0;
    uniDeviceName.MaximumLength = sizeof(NT_DEVICE_NAME2) + 3 * sizeof(WCHAR);
    RtlAppendUnicodeToString( &uniDeviceName, NT_DEVICE_NAME2);
    //DoTraceMessage(FLAG_ONE, "uniDeviceName = %wZ len=%d max=%d", &uniDeviceName, uniDeviceName.Length, uniDeviceName.MaximumLength);
    RtlAppendUnicodeStringToString( &uniDeviceName, &uniDevCount);
    //DoTraceMessage(FLAG_ONE, "after append nt_device name2 = %wZ", &uniDeviceName);
    DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("b4 IoCreateDevice uniDeviceName: %wZ len=%d max=%d\n", &uniDeviceName,  uniDeviceName.Length, uniDeviceName.MaximumLength));
    ExFreePool( uniDevCount.Buffer );

    //
    // Create the device object
    //
    status = IoCreateDevice(
                DriverObject,
                sizeof(DEVICE_EXTENSION),
                &uniDeviceName,
                FILE_DEVICE_DISK , // old value =FILE_DEVICE_VIRTUAL_DISK corrected proposed by MS 
                (FILE_DEVICE_SECURE_OPEN),
                FALSE,                 // This isn't an exclusive device
                &functionDeviceObject
                );


    if (!NT_SUCCESS(status)) {
	//DoTraceMessage(FLAG_ONE, "IoCreateDevice error: 0x%x", status);
        DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, ("IoCreateDevice error: 0x%x\n", status) );
        return status;        
    }
    //DoTraceMessage(FLAG_ONE, "FDO created successfully (%p)", functionDeviceObject);


    DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_INFO, ("FDO created successfully (%p)\n", functionDeviceObject) );
    devExt = functionDeviceObject->DeviceExtension;
    //DoTraceMessage(FLAG_ONE, "device Extension = %p", devExt);

    RtlZeroMemory( devExt, sizeof(DEVICE_EXTENSION) );
    //DoTraceMessage(FLAG_ONE, "device Extension zerorise");




    //concatenate volume filename
    absFileName.Buffer = ExAllocatePool2( 
			    POOL_FLAG_PAGED,
                            sizeof(L"\\DosDevices\\") + MAX_PATH2 * sizeof(WCHAR),
                            RAMDISK_TAG_GENERAL);
    absFileName.Length = 0;
    absFileName.MaximumLength = sizeof(L"\\DosDevices\\") + MAX_PATH2 * sizeof(WCHAR);
    RtlAppendUnicodeToString( &absFileName, L"\\DosDevices\\");
    //DoTraceMessage(FLAG_ONE, "RtlAppendUnicodeToString absFilename = %wZ len=%d", &absFileName, absFileName.Length);
    RtlAppendUnicodeToString( &absFileName, pCreateVol->FilePath);
    //DoTraceMessage(FLAG_ONE, "RtlAppendUnicodeToString absFilename = %wZ", &absFileName);
    DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("RtlAppendUnicodeToString absFilename: %wZ\n", &absFileName));

    InitializeObjectAttributes( &fileAttributes,
                                &absFileName,
                                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                NULL, // RootDirectory
                                NULL // SecurityDescriptor
                                );
    //DoTraceMessage(FLAG_ONE, "after init object attribute");

    status = ZwCreateFile (
                    &devExt->FileHandle,
                    SYNCHRONIZE | GENERIC_WRITE | GENERIC_READ,
                    &fileAttributes,
                    &ioStatus,
                    0,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_READ,
                    FILE_OPEN_IF,
                    FILE_SYNCHRONOUS_IO_NONALERT |FILE_NON_DIRECTORY_FILE ,
                    NULL,// eabuffer
                    0// ealength
                    );
    //1024 * 1024
    // | FILE_RANDOM_ACCESS
    if (!NT_SUCCESS(status)) {
	    //DoTraceMessage(FLAG_ONE, "ZwCreateFile return error %lX", ioStatus.Status);
	    DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("ZwCreateFile return error %lX ioStatus:%lX\n", status, ioStatus.Status));
    	return STATUS_INSUFFICIENT_RESOURCES;
    }
    //DoTraceMessage(FLAG_ONE, "after zwCreatefile");
    if (pCreateVol->DiskSize.QuadPart == 0 ) {
    	// read disksize, driveletter, bytespersector, sectorspertrack, tracksperCyl,
	    //	cylinders, mediatype
	    PCREATE_VOL	pCreateVol2;
	    FILE_POSITION_INFORMATION   position;

	    //DoTraceMessage(FLAG_ONE, "Disksize zero");
	    DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("Disksize zero (not specified)\n"));
	    pCreateVol2 = ExAllocatePool2(              //get the volume info form sector 0
                            POOL_FLAG_PAGED,
                            sizeof(CREATE_VOL) - MAX_PATH2 * sizeof(WCHAR),         //excluding FilePath
                            RAMDISK_TAG_GENERAL);
    	position.CurrentByteOffset.QuadPart = 0;
	
	    status = ZwSetInformationFile(devExt->FileHandle,
                             &ioStatus,
                             &position,
                             sizeof(FILE_POSITION_INFORMATION),
                             FilePositionInformation);
	    if (NT_SUCCESS(status)) {
            status = ZwReadFile (devExt->FileHandle,
                                NULL,//   Event,
                                NULL,// PIO_APC_ROUTINE  ApcRoutine
                                NULL,// PVOID  ApcContext
                                &ioStatus,
                                pCreateVol2,
                                (ULONG)sizeof(CREATE_VOL) - MAX_PATH2 * sizeof(WCHAR),
                                0, // ByteOffset
                                NULL // Key
                                );

    	    //DoTraceMessage(FLAG_ONE, "ZwReadFile2 status=%X", status);
	        DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("ZwReadFile Zero offset status=%X CreateVol2 size:%d \n", status, (sizeof(CREATE_VOL) - MAX_PATH2 * sizeof(WCHAR))));

	    }
	    else {
	        //DoTraceMessage(FLAG_ONE, "ZwSetInformationFile2 fail status=%X", status);
	    	DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("ZwSetInformationFile2 fail status=%X ioStatus:%X\n", status, ioStatus.Status));
		    // NO RETURN ????
	    }
	    devExt->DiskSize = pCreateVol2->DiskSize;
	    devExt->Cylinders = pCreateVol2->Cylinders;
	    devExt->TracksPerCyl = pCreateVol2->TracksPerCyl;
	    devExt->SectorsPerTrk = pCreateVol2->SectorsPerTrk;
	    devExt->BytesPerSec = pCreateVol2->BytesPerSec;
	//need to compare pCreateVol->DriveLetter and pCreateVol2->DriveLetter
	// and take pCreateVol->DriveLetter (user specified)
	// write back new DriveLetter if createsymbolicLink successful
    	RtlStringCchCopyW(devExt->DriveLetter,
				sizeof(pCreateVol2->DriveLetter),
				pCreateVol2->DriveLetter);
    	devExt->nonloop = pCreateVol->nonloop;		//from user
        DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("nonloop: %d\n", pCreateVol->nonloop));
	    DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("From Offset 0: DiskSize=%I64u Cyl=%d Track=%d Sector=%d Byte/Sec=%d\n",
	    	pCreateVol2->DiskSize.QuadPart,
	    	pCreateVol2->Cylinders,
	    	pCreateVol2->TracksPerCyl,
	    	pCreateVol2->SectorsPerTrk,
        	pCreateVol2->BytesPerSec)); 
	    DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("DriveLetter %x %x %x %x.\n", devExt->DriveLetter[0],devExt->DriveLetter[1],devExt->DriveLetter[2], devExt->DriveLetter[2]));

    	ExFreePool( pCreateVol2 );
    }
    else {
	    //write disksize, driveletter, bytespersector, sectorspertrack, tracksperCyl,
	    //	cylinders, mediatype
	    //write a few more sectors
	    PVOID		pVolume;
	    PCREATE_VOL	pCreateVol2;
	    USHORT		sector;
	    FILE_POSITION_INFORMATION   position;

	    pVolume = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            pCreateVol->BytesPerSec,
                            RAMDISK_TAG_GENERAL);
    	RtlZeroMemory( pVolume, pCreateVol->BytesPerSec );
    	//check if it zero or RAMDISK_TAG_GENERAL
	    for (sector=1; sector <  10; sector++) {
	        position.CurrentByteOffset.QuadPart = sector * pCreateVol->BytesPerSec;
	
	        status = ZwSetInformationFile(devExt->FileHandle,
                                 &ioStatus,
                                 &position,
                                 sizeof(FILE_POSITION_INFORMATION),
                                 FilePositionInformation);
	        if (NT_SUCCESS(status)) {
		        status = ZwWriteFile(devExt->FileHandle,
                                    NULL,//   Event,
                                    NULL,// PIO_APC_ROUTINE  ApcRoutine
                                    NULL,// PVOID  ApcContext
                                    &ioStatus,
	                            pVolume,
	                            pCreateVol->BytesPerSec,
                                    0, // ByteOffset
                                    NULL // Key
                                    );
		    //DoTraceMessage(FLAG_ONE, "ZwWriteFile status=%X", status);
	        }
	        else {
		        //DoTraceMessage(FLAG_ONE, "ZwSetInformationFile fail status=%X", status);
	        }
        }
	    pCreateVol2 = pVolume;                              //give allocated space to pCreateVol2
	    pCreateVol2->DiskSize = pCreateVol->DiskSize;       //and transfer user value to pCreateVol2
	    pCreateVol2->Cylinders = pCreateVol->Cylinders;
	    pCreateVol2->TracksPerCyl = pCreateVol->TracksPerCyl;
	    pCreateVol2->SectorsPerTrk = pCreateVol->SectorsPerTrk;
        pCreateVol2->BytesPerSec = pCreateVol->BytesPerSec;
	    RtlStringCchCopyW(pCreateVol2->DriveLetter,
				    sizeof(pCreateVol2->DriveLetter),
				    pCreateVol->DriveLetter);
	    //DoTraceMessage(FLAG_ONE, "going to write to vol DriveLetter = %wZ", &(pCreateVol2->DriveLetter));

	    position.CurrentByteOffset.QuadPart = 0;
	
	    status = ZwSetInformationFile(devExt->FileHandle,
                                 &ioStatus,
                                 &position,
                                 sizeof(FILE_POSITION_INFORMATION),
                                 FilePositionInformation);
	    if (NT_SUCCESS(status)) {
	        status = ZwWriteFile(devExt->FileHandle,
                                    NULL,//   Event,
                                    NULL,// PIO_APC_ROUTINE  ApcRoutine
                                    NULL,// PVOID  ApcContext
                                    &ioStatus,
	                            pVolume,
	                            pCreateVol->BytesPerSec,
                                    0, // ByteOffset
                                    NULL // Key
                                    );
	        //DoTraceMessage(FLAG_ONE, "ZwWriteFile2 status=%X", status);
            DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("ZwWriteFile pCreateVol2 status=%X", status));
	    }
	    else {
	        //DoTraceMessage(FLAG_ONE, "ZwSetInformationFile2 fail status=%X", status);
            DBGPRINT(DBG_COMP_NONE, DBG_LEVEL_VERBOSE, ("ZwSetInformationFile2 fail status=%X", status));
	    }
	    //keep also in devExt
	    devExt->DiskSize = pCreateVol->DiskSize;
	    devExt->Cylinders = pCreateVol->Cylinders;
	    devExt->TracksPerCyl = pCreateVol->TracksPerCyl;
	    devExt->SectorsPerTrk = pCreateVol->SectorsPerTrk;
	    devExt->BytesPerSec = pCreateVol->BytesPerSec;
//	RtlStringCchCopyW(devExt->DriveLetter,
//				sizeof(pCreateVol2->DriveLetter),
//				pCreateVol->DriveLetter);
        devExt->DriveLetter[0] = pCreateVol->DriveLetter[0];
        devExt->DriveLetter[1] = pCreateVol->DriveLetter[1];
        devExt->DriveLetter[2] = pCreateVol->DriveLetter[2];
	    devExt->nonloop = pCreateVol->nonloop;		//from user
	    ExFreePool( pVolume );
    } //write
    devExt->Flags |= FLAG_VOL_DEVICE;


    // Allocate buffer for storing the drive letter
//    devExt->DiskRegInfo.DriveLetter.Buffer =  ExAllocatePoolWithTag( 
//                            PagedPool, 
//                            DRIVE_LETTER_LENGTH,
//                            RAMDISK_TAG_GENERAL);
    
//    if ( devExt->DiskRegInfo.DriveLetter.Buffer == NULL ) {
//	DoTraceMessage(FLAG_ONE, "Can't allocate memory for drive letter");
        //DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, ("Can't allocate memory for drive letter\n") );
//        RamDiskCleanUp( functionDeviceObject );
//        return STATUS_INSUFFICIENT_RESOURCES;
//    }
//    devExt->DiskRegInfo.DriveLetter.MaximumLength = DRIVE_LETTER_LENGTH;

    // 
    // Get the disk parameters from the registry
    //

//    RamDiskQueryDiskRegParameters( &driverExtension->RegistryPath, &devExt->DiskRegInfo  );
//    RamDiskQueryDiskGeometry(functionDeviceObject );
    //DoTraceMessage(FLAG_ONE, "af RamDiskQueryDiskRegParameters");


    //devExt->PhysicalDeviceObject = PhysicalDeviceObject;  // Save PDO pointer
    devExt->DeviceObject = functionDeviceObject;          // Save device object pointer
    devExt->DevState = WORKING;                           // Ramdisk.c starts in Stopped state
//    devExt->startPrint = FALSE;				//dont print bfore/after value until set to true
//    devExt->donePrint = FALSE;
    //DoTraceMessage(FLAG_ONE, "b4 IoInitializeRemoveLock");

    IoInitializeRemoveLock ( &devExt->RemoveLock, 
                            REMLOCK_TAG, 
                            REMLOCK_MAXIMUM, 
                            REMLOCK_HIGHWATER);

    // Set device flags

    functionDeviceObject->Flags |= DO_POWER_PAGABLE;
    functionDeviceObject->Flags |= DO_DIRECT_IO;

    //
    // Open file for volume
    //
    //RtlInitUnicodeString( &absFileName, L"\\DosDevices\\C:\\Temp\\fs.dat" );



    // Format the disk
    //DoTraceMessage(FLAG_ONE, "b4 RamDiskFormatDisk");
    //RamDiskFormatDisk( functionDeviceObject );

    // Create symbolic link, which is the drive letter for the ramdisk
    //DoTraceMessage(FLAG_ONE, "b4 ExAllocatePoolWithTag symboliclink");

    devExt->SymbolicLink.Buffer = ExAllocatePool2( 
                            POOL_FLAG_PAGED,
                            sizeof(DOS_DEVICE_NAME2) + 3 * sizeof(WCHAR),
                            RAMDISK_TAG_GENERAL);		//free when cleanup

    if ( devExt->SymbolicLink.Buffer == NULL ) {
	//DoTraceMessage(FLAG_ONE, "Can't allocate memory for symbolic link");

        DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, ("Can't allocate memory for symbolic link\n") );
        RamDiskCleanUp( functionDeviceObject );
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlInitUnicodeString( &uniWin32Name, DOS_DEVICE_NAME2 );	//L"\\DosDevices\\"
    //DoTraceMessage(FLAG_ONE, "after init DOS_DEVICE_NAME2 = %wZ", &uniWin32Name);

    devExt->SymbolicLink.MaximumLength = sizeof(DOS_DEVICE_NAME2) + 3 * sizeof(WCHAR);
    devExt->SymbolicLink.Length = uniWin32Name.Length;

    RtlCopyUnicodeString( &(devExt->SymbolicLink), &uniWin32Name );
    // try pCreateVol->DriveLetter try devExt->DriveLetter instead of devExt->DiskRegInfo.DriveLetter
    RtlAppendUnicodeToString(&(devExt->SymbolicLink), devExt->DriveLetter);
    //RtlAppendUnicodeStringToString( &(devExt->SymbolicLink), &(devExt->DiskRegInfo.DriveLetter) );

    //DoTraceMessage(FLAG_ONE, "Creating drive letter = %wZ", &(devExt->SymbolicLink));

    DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_NOTIFY, ("Creating drive letter = %wZ\n",&(devExt->SymbolicLink) ) );
    //     status = IoSetDeviceInterfaceState(&(devExt->SymbolicLink), TRUE);
    // status = C000000D STATUS_INVALID_PARAMETER 
    //	 if(!NT_SUCCESS(status)) {
    //		DoTraceMessage(FLAG_ONE, "IoSetDeviceInterfaceState failed %X", status);
    //interval.QuadPart = (LONGLONG) -1000000;
    //		KeDelayExecutionThread(KernelMode, FALSE, &interval );
    //	 }
    // Create a drive letter from our device name to a name in the Win32 namespace.
    //DoTraceMessage(FLAG_ONE, "b4 IoCreateSymbolicLink");
    DBGPRINT(DBG_COMP_INIT, DBG_LEVEL_NOTIFY, ("b4 IoCreateSymbolicLink p1 = %wZ p2 = %wZ\n", &(devExt->SymbolicLink), &uniDeviceName));

    //status = IoCreateSymbolicLink( &devExt->SymbolicLink, &uniDeviceName );
    status = IoCreateUnprotectedSymbolicLink(&devExt->SymbolicLink, &uniDeviceName);
    ExFreePool( uniDeviceName.Buffer );
    ExFreePool( absFileName.Buffer );

    if (!NT_SUCCESS(status)) {
	//DoTraceMessage(FLAG_ONE, "IoCreateSymbolicLink error: 0x%x", status);

        DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_ERROR, ("IoCreateSymbolicLink error: 0x%x\n", status) );
        RamDiskCleanUp( functionDeviceObject );
        return status;
    }


    devExt->Flags |= FLAG_LINK_CREATED;


    //createPDOAttach(functionDeviceObject);



    driverExtension->DeviceInitialized = TRUE;






    // Clear DO_DEVICE_INITIALIZING flag

    functionDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    //DoTraceMessage(FLAG_ONE, "IoCreateDevice reference count = %d\n", functionDeviceObject->ReferenceCount);
    //DoTraceMessage(FLAG_ONE, "AddDevice - OUT. Fdo=(%p) LowerDevice=(%p)",  functionDeviceObject, devExt->LowerDeviceObject);


    DBGPRINT( DBG_COMP_INIT, DBG_LEVEL_VERBOSE, ("AddDevice - OUT. Fdo=(%p) LowerDevice=(%p)\n",
        functionDeviceObject, devExt->LowerDeviceObject ) );

    return status;
}  // End of RamDiskAddDevice()



VOID
PrintIrpInfo(
    PIRP Irp)
{
    PIO_STACK_LOCATION  irpSp;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    SIOCTL_KDPRINT(("\tIrp->AssociatedIrp.SystemBuffer = 0x%p\n",
        Irp->AssociatedIrp.SystemBuffer));
    SIOCTL_KDPRINT(("\tIrp->UserBuffer = 0x%p\n", Irp->UserBuffer));
    SIOCTL_KDPRINT(("\tirpSp->Parameters.DeviceIoControl.Type3InputBuffer = 0x%p\n",
        irpSp->Parameters.DeviceIoControl.Type3InputBuffer));
    SIOCTL_KDPRINT(("\tirpSp->Parameters.DeviceIoControl.InputBufferLength = %d\n",
        irpSp->Parameters.DeviceIoControl.InputBufferLength));
    SIOCTL_KDPRINT(("\tirpSp->Parameters.DeviceIoControl.OutputBufferLength = %d\n",
        irpSp->Parameters.DeviceIoControl.OutputBufferLength ));
    return;
}

VOID
PrintChars(
    __in_ecount(CountChars) PCHAR BufferAddress,
    __in size_t CountChars
    )
{
    PAGED_CODE();

    if (CountChars) {

        while (CountChars--) {

            if (*BufferAddress > 31
                 && *BufferAddress != 127) {

                KdPrint (( "%c", *BufferAddress) );

            } else {

                KdPrint(( ".") );

            }
            BufferAddress++;
        }
        KdPrint (("\n"));
    }
    return;
}


