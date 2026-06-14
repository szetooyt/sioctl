/*++
Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    install.c

Abstract:

    Win32 routines to dynamically load and unload a Windows NT kernel-mode
    driver using the Service Control Manager APIs.

Environment:

    User mode only

--*/


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "ioctlcode.h"
//#include "sioctl.h"

BOOLEAN
InstallDriver(
    __in SC_HANDLE  SchSCManager,
    __in LPCTSTR    DriverName,
    __in LPCTSTR    ServiceExe
    );


BOOLEAN
RemoveDriver(
    __in SC_HANDLE  SchSCManager,
    __in LPCTSTR    DriverName
    );

BOOLEAN
StartDriver(
    __in SC_HANDLE  SchSCManager,
    __in LPCTSTR    DriverName
    );

BOOLEAN
StopDriver(
    __in SC_HANDLE  SchSCManager,
    __in LPCTSTR    DriverName,
__in LPCTSTR    ServiceExe
    );
VOID
printError(
    __in LPCTSTR	errorString1,
    __in DWORD		err
);
BOOLEAN
SetupDriverName(
    __in LPCTSTR    DriverName,
    __inout_bcount_full(BufferLength) PCHAR DriverLocation,
    __in ULONG BufferLength
    );


BOOLEAN
InstallDriver(
    __in SC_HANDLE  SchSCManager,
    __in LPCTSTR    DriverName,
    __in LPCTSTR    ServiceExe
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    SC_HANDLE   schService;
    DWORD       err;

    //
    // NOTE: This creates an entry for a standalone driver. If this
    //       is modified for use with a driver that requires a Tag,
    //       Group, and/or Dependencies, it may be necessary to
    //       query the registry for existing driver information
    //       (in order to determine a unique Tag, etc.).
    //
    printf("InstallDriver DriverName = %s\n", DriverName);
    printf("InstallDriver ServiceExe = %s\n", ServiceExe);
    //
    // Create a new a service object.
    //

    schService = CreateService(SchSCManager,           // handle of service control manager database
                               DriverName,             // address of name of service to start
                               DriverName,             // address of display name
                               SERVICE_ALL_ACCESS,     // type of access to service
                               SERVICE_KERNEL_DRIVER,  // type of service
                               SERVICE_DEMAND_START,   // when to start service
                               SERVICE_ERROR_NORMAL,   // severity if service fails to start
                               ServiceExe,             // address of name of binary file
                               NULL,                   // service does not belong to a group
                               NULL,                   // no tag requested
                               NULL,                   // no dependency names
                               NULL,                   // use LocalSystem account
                               NULL                    // no password for service account
                               );

    if (schService == NULL) {

        err = GetLastError();

        if (err == ERROR_SERVICE_EXISTS) {

            //
            // Ignore this error.
            //

            return TRUE;

        } else {

            printf("CreateService failed!  Error = %d \n", err );

            //
            // Indicate an error.
            //

            return  FALSE;
        }
    }

    //
    // Close the service object.
    //

    if (schService) {

        CloseServiceHandle(schService);
    }

    //
    // Indicate success.
    //

    return TRUE;

}   // InstallDriver

BOOLEAN
ManageDriver(
    __in LPCTSTR  DriverName,
    __in USHORT   Function
    )
{

    SC_HANDLE   schSCManager;

    BOOLEAN rCode = TRUE;
    TCHAR ServiceName[MAX_PATH];

    if (!SetupDriverName(DriverName,ServiceName, sizeof(ServiceName))) {
	return FALSE;
    }
    //
    // Insure (somewhat) that the driver and service names are valid.
    //

    if (!DriverName || !ServiceName) {

        printf("Invalid Driver or Service provided to ManageDriver() \n");

        return FALSE;
    }

    //
    // Connect to the Service Control Manager and open the Services database.
    //

    schSCManager = OpenSCManager(NULL,                   // local machine
                                 NULL,                   // local database
                                 SC_MANAGER_ALL_ACCESS   // access required
                                 );

    if (!schSCManager) {

        printf("Open SC Manager failed! Error = %d \n", GetLastError());

        return FALSE;
    }

    //
    // Do the requested function.
    //

    switch( Function ) {

        case DRIVER_FUNC_INSTALL:

            //
            // Install the driver service.
            //

            if (InstallDriver(schSCManager,
                              DriverName,
                              ServiceName
                              )) {

                //
                // Start the driver service (i.e. start the driver).
                //

                rCode = StartDriver(schSCManager,
                                    DriverName
                                    );

            } else {

                //
                // Indicate an error.
                //

                rCode = FALSE;
            }

            break;

        case DRIVER_FUNC_REMOVE:

            //
            // Stop the driver.
            //

            StopDriver(schSCManager,
                       DriverName,
			ServiceName
                       );

            //
            // Remove the driver service.
            //

            RemoveDriver(schSCManager,
                         DriverName
                         );

            //
            // Ignore all errors.
            //

            rCode = TRUE;

            break;

        default:

            printf("Unknown ManageDriver() function. \n");

            rCode = FALSE;

            break;
    }

    //
    // Close handle to service control manager.
    //

    if (schSCManager) {

        CloseServiceHandle(schSCManager);
    }

    return rCode;

}   // ManageDriver


BOOLEAN
RemoveDriver(
    __in SC_HANDLE    SchSCManager,
    __in LPCTSTR      DriverName
    )
{
    SC_HANDLE   schService;
    BOOLEAN     rCode;
    
    //
    // Open the handle to the existing service.
    //
    printf("RemoveDriver %s\n", DriverName);

    schService = OpenService(SchSCManager,
                             DriverName,
                             SERVICE_ALL_ACCESS
                             );

    if (schService == NULL) {
	printError("OpenService failed!", GetLastError());
        //
        // Indicate error.
        //

        return FALSE;
    }

    //
    // Mark the service for deletion from the service control manager database.
    //

    if (DeleteService(schService)) {

        //
        // Indicate success.
        //

        rCode = TRUE;

    } else {

        printf("DeleteService failed!  Error = %d \n", GetLastError());

        //
        // Indicate failure.  Fall through to properly close the service handle.
        //

        rCode = FALSE;
    }

    //
    // Close the service object.
    //

    if (schService) {

        CloseServiceHandle(schService);
    }

    return rCode;

}   // RemoveDriver



BOOLEAN
StartDriver(
    __in SC_HANDLE    SchSCManager,
    __in LPCTSTR      DriverName
    )
{
    SC_HANDLE   schService;
    DWORD       err;

    //
    // Open the handle to the existing service.
    //
    printf("StartDriver : DriverName = %s\n", DriverName);
    schService = OpenService(SchSCManager,
                             DriverName,
                             SERVICE_ALL_ACCESS
                             );

    if (schService == NULL) {

	err = GetLastError();
	switch (err) {
	    case ERROR_SERVICE_DOES_NOT_EXIST:
		printf("OpenService failed!  SERVICE_DOES_NOT_EXIST\n");
		break;
	    default:
		printf("OpenService failed!  Error = %d \n", GetLastError());

	} //switch

        //
        // Indicate failure.
        //

        return FALSE;
    }

    //
    // Start the execution of the service (i.e. start the driver).
    //

    if (!StartService(schService,     // service identifier
                      0,              // number of arguments
                      NULL            // pointer to arguments
                      )) {

        err = GetLastError();

        if (err == ERROR_SERVICE_ALREADY_RUNNING) {
	    printf("StartService ERROR_SERVICE_ALREADY_RUNNING err:%d\n", err);
            //
            // Ignore this error.
            //

            return TRUE;

        } else {
	    printError("StartService failed!", GetLastError());
            //
            // Indicate failure.  Fall through to properly close the service handle.
            //

            return FALSE;
        }

    }

    //
    // Close the service object.
    //

    if (schService) {

        CloseServiceHandle(schService);
    }

    return TRUE;

}   // StartDriver



BOOLEAN
StopDriver(
    __in SC_HANDLE    SchSCManager,
    __in LPCTSTR      DriverName,
    __in LPCTSTR    ServiceExe
    )
{
    BOOLEAN         rCode = TRUE;
    SC_HANDLE       schService;
    SERVICE_STATUS  serviceStatus;
    DWORD	    err;
    //
    // Open the handle to the existing service.
    //
    printf("StopDriver %s\n", DriverName);

    schService = OpenService(SchSCManager,
                             DriverName,
                             SERVICE_ALL_ACCESS
                             );

    if (schService == NULL) {
	err = GetLastError();
	if (err == ERROR_SERVICE_DOES_NOT_EXIST ) {
		InstallDriver(SchSCManager,
                              DriverName,
                              ServiceExe
                              );
		// i dont want to return if create service succeed
	}
	printError("OpenService failed!", err);
        
        return FALSE;
    }

    //
    // Request that the service stop.
    //

    if (ControlService(schService,
                       SERVICE_CONTROL_STOP,
                       &serviceStatus
                       )) {

        //
        // Indicate success.
        //

        rCode = TRUE;

    } else {
	printError("ControlService failed!", GetLastError());


        //
        // Indicate failure.  Fall through to properly close the service handle.
        //

        rCode = FALSE;
    }

    //
    // Close the service object.
    //

    if (schService) {

        CloseServiceHandle (schService);
    }

    return rCode;

}   //  StopDriver

BOOLEAN
SetupDriverName(
    __in LPCTSTR    DriverName,
    __inout_bcount_full(BufferLength) PCHAR DriverLocation,
    __in ULONG BufferLength
    )
{
    HANDLE fileHandle;
    DWORD driverLocLen = 0;

    //
    // Get the current directory.
    //

    driverLocLen = GetCurrentDirectory(BufferLength,
                                       DriverLocation
                                       );

    if (driverLocLen == 0) {

        printf("GetCurrentDirectory failed!  Error = %d \n", GetLastError());

        return FALSE;
    }

    //
    // Setup path name to driver file.
    //
    if (FAILED( StringCbCat(DriverLocation, BufferLength, "\\") )) {
        return FALSE;
    }
    if (FAILED( StringCbCat(DriverLocation, BufferLength, DriverName) )) {
        return FALSE;
    }
    if (FAILED( StringCbCat(DriverLocation, BufferLength, ".sys") )) {
        return FALSE;
    }
    //
    // Insure driver file is in the specified directory.
    //

    if ((fileHandle = CreateFile(DriverLocation,
                                 GENERIC_READ,
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL
                                 )) == INVALID_HANDLE_VALUE) {


        printf("%s.sys is not loaded.\n", DriverName);

        //
        // Indicate failure.
        //

        return FALSE;
    }

    //
    // Close open file handle.
    //

    if (fileHandle) {

        CloseHandle(fileHandle);
    }

    //
    // Indicate success.
    //

    return TRUE;


}   // SetupDriverName

VOID
printError(
    __in LPCTSTR	errorString1,
    __in DWORD		err
)
{
	switch (err) {
	    case ERROR_SERVICE_DOES_NOT_EXIST:
		printf("%s  SERVICE_DOES_NOT_EXIST error:%d\n", errorString1, err);
		break;
	    case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
		printf("%s  ERROR_SERVICE_CANNOT_ACCEPT_CTRL error:%d\n", errorString1, err);
		break;
	    case ERROR_SERVICE_MARKED_FOR_DELETE:
		printf("%s  ERROR_SERVICE_MARKED_FOR_DELETE error:%d\n", errorString1, err);
		break;
	    default:
		printf("%s Error = %d \n", errorString1, err );
	    } //switch
}
