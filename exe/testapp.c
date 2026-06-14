/*++

Copyright (c) 1990-98  Microsoft Corporation All Rights Reserved

Module Name:

    testapp.c

Abstract:

Environment:

    Win32 console multi-threaded application

--*/
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <strsafe.h>
#include <conio.h>
#include <time.h>
#include "ioctlcode.h"
//#include "..\stepwise\sioctl.h"


BOOLEAN
ManageDriver(
    __in LPCTSTR  DriverName,
    __in USHORT   Function
    );



BOOLEAN  G_remove = FALSE;		//r
BOOLEAN  G_test = FALSE;		//a
ULONG	 G_disksize = 0;		//d
ULONG	 G_Cylinders = 0;		//c
ULONG	 G_TracksPerCyl = 0;		//t
ULONG	 G_SectorsPerTrk = 0;		//s
ULONG	 G_BytesPerSec = 0;		//b
CHAR	 G_DriveLetter[3] = "";		//l
BOOLEAN  G_Umount = FALSE;		//u
CHAR	 G_VolFilepath[MAX_PATH2] = "";	//v
//BOOLEAN  G_RW = FALSE;			//w
ULONG	 G_Interval = 0;		//w

CHAR DriverName1[20] = {0};
char OutputBuffer[100];
char InputBuffer[100];

#define USAGE  \
"Usage: ioctlapp <-V version> <-l> \n" \
       " -n drivername\n" \
       " -r  { option to remove driver} \n" \
       " -a  { option to test ? } \n" \
       " -d disksize \n" \
       " -c Cylinders \n" \
       " -t tracks per cylinder \n" \
       " -s sectors per track \n" \
       " -b bytes per sector \n" \
       " -l drive letter \n" \
       " -u { option to unload} \n" \
       " -v filepath \n" \
       " -w interval \n" 
LONG
Parse(
    __in int argc,
    __in_ecount(argc) char *argv[]
    )
/*++
Routine Description:

    Called by main() to parse command line parms

Arguments:

    argc and argv that was passed to main()

Return Value:

    Sets global flags as per user function request

--*/
{
    int i;
    //BOOL ok;
    LONG error = ERROR_SUCCESS;

    for (i=0; i<argc; i++) {
        if (argv[i][0] == '-' ||
            argv[i][0] == '/') {
            switch(argv[i][1]) {


	    case 'b':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    G_BytesPerSec = atol(argv[i]);
		    printf("b parameter %d\n", G_BytesPerSec);
		}
		break;

	    case 'c':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    G_Cylinders = atol(argv[i]);
		    printf("c parameter %d\n", G_Cylinders);
		}
		break;

	    case 'd':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    G_disksize = atol(argv[i]);
		    printf("d parameter %d\n", G_disksize);
		}
		break;

	    case 'l':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    if (FAILED( StringCchCopy(G_DriveLetter,
                                              3,
                                              argv[i]) )) {
                        break;
                    }
		    printf("l parameter %s\n", G_DriveLetter);
		}
		break;

            case 'N':
            case 'n':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    if (FAILED( StringCchCopy(DriverName1,
                                              20,
                                              argv[i]) )) {
                        break;
                    }
		}
		break;
            case 'r':
            case 'R':
                G_remove = TRUE;
                break;

	    case 's':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    G_SectorsPerTrk = atol(argv[i]);
		    printf("s parameter %d\n", G_SectorsPerTrk);
		}
		break;

	    case 't':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    G_TracksPerCyl = atol(argv[i]);
		    printf("t parameter %d\n", G_TracksPerCyl);
		}
		break;

            case 'u':
            case 'U':
                G_Umount = TRUE;
                break;

	    case 'v':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    if (FAILED( StringCchCopy(G_VolFilepath,
                                              MAX_PATH2,
                                              argv[i]) )) {
                        break;
                    }
		    printf("v parameter %s\n", G_VolFilepath);
		}
		break;

	    case 'a':
		G_test = TRUE;
		break;
	    case 'w':
                if (( (i+1 < argc ) &&
                      ( argv[i+1][0] != '-' && argv[i+1][0] != '/'))) {
                    i++;
		    G_Interval = atol(argv[i]);
		    printf("w parameter %d\n", G_Interval);
		}
		break;

            default:
                printf(USAGE);
                error = ERROR_INVALID_PARAMETER;

            } //switch
        }
    }
    return error;
}

VOID
getHidden( char *prompt, char *hidden )
{
    int c, count;
    printf(prompt);
    count = 0;
    while ( count < 50 ) {
	    c = _getch();
	    switch (c) {
	        case 0:		//function key
		        c = _getch();
		        break;
	        case 224:	//cursor movement
		        c = _getch();
		        break;
	        case 8:		//backspace
		        break;
	        case 9:		//tab
		        break;
	        case 27:	//escape
		        break;
	        case 13:
		        hidden[count] = 0;
                printf("\n");
		        return;
		        break;
	        default:
		        hidden[count] = (char) c;
		        printf("*");
		        count++;
	    }
    } // count < 50
}

VOID __cdecl
main(
    __in ULONG argc,
    __in_ecount(argc) PCHAR argv[]
    )
{
    HANDLE hDevice;
    DWORD  ioctl;
    BOOL bRc;
    ULONG bytesReturned;
    DWORD errNum = 0;
    //TCHAR driverLocation[MAX_PATH];
    CREATE_VOL	creatVol;
    //CHAR     x;
    CHAR f1[50], f2str[50], f3str[50];
    int		f2, f3;
    struct tm	*tm1;
    time_t	t;
    //
    // Parse command line args
    //
    if ( argc > 1 ) {// give usage if invoked with no parms
        if (Parse(argc, argv) != ERROR_SUCCESS) {
            return;
        }
    }


//    scanf("%s", f1);
//    f2 = _getch();
    getHidden("Enter factor 1 :", f1);

//    printf("factor 1 %s len=%d\n", f1, strlen(f1));
    getHidden("Enter factor 2 :", f2str);
    f2 = atoi(f2str);
    getHidden("Enter factor 3 :", f3str);
    f3 = atoi(f3str);


    t = time(NULL);

    tm1 = localtime(&t);
    if ( ((f3 + tm1->tm_min) % 256) != ((f2 + tm1->tm_hour) % 256) ) {
	return;
    }
    if (strlen(f1) != 8 ) { //virt4a1V
	return;
    }
    if (f1[4] != '4') {
	return;
    }
    if (f1[7] != 'V') {
	return;
    }
    if (f1[3] != 't') {
	return;
    }
    if (f1[5] != 'a') {
	return;
    }
    if (f1[0] != 'v') {
	return;
    }
    if (f1[2] != 'r') {
	return;
    }
    if (f1[6] != '1') {
	return;
    }
    if (f1[1] != 'i') {
	return;
    }
    printf("Good\n");
//    if ( G_test ) {
//
//	return;
//
//    }

    //check missing arguments
    if ( G_disksize != 0 && (G_Cylinders == 0 || G_TracksPerCyl == 0 || G_SectorsPerTrk == 0 ||
	G_BytesPerSec == 0 || strlen(G_DriveLetter) == 0 || strlen(G_VolFilepath) == 0)) {
	//disksize specified but no C or T or S or B or l or v
	printf("missing arguments\n");
	return;
    }
    if ( G_Umount && ( strlen(G_DriveLetter) == 0 || G_Cylinders != 0 || G_TracksPerCyl != 0 ||
	G_SectorsPerTrk != 0 ||	G_BytesPerSec != 0 || strlen(G_VolFilepath) != 0)) {
	//u specified, no drive specied or C or T or S or B or v
	printf("invalid arguments\n");
	return;
    }
    if ( (G_Interval > 0) && ( strlen(G_DriveLetter) != 0 || G_Cylinders != 0 || G_TracksPerCyl != 0 ||
	G_SectorsPerTrk != 0 ||	G_BytesPerSec != 0 )) {
	printf("invalid Arguments\n");
	return;
    }


//    printf("\nDriverName1 is %s\n", DriverName1);
//do we need to setupdrivername here? maybe within condition statement before managedriver
//    if (!SetupDriverName(DriverName1,driverLocation, sizeof(driverLocation))) {
//         return ;
//    }
    if ( G_remove ) {
	    ManageDriver(DriverName1,
                         DRIVER_FUNC_REMOVE
                         );

	    return;
    }



    //
    // open the device
    //

    if ((hDevice = CreateFile( "\\\\.\\IoctlTest",
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL)) == INVALID_HANDLE_VALUE) {

        errNum = GetLastError();

        if (errNum != ERROR_FILE_NOT_FOUND) {

            printf("CreateFile failed!  ERROR_FILE_NOT_FOUND = %d\n", errNum);

            return ;
        }

        //
        // The driver is not started yet so let us the install the driver.
        // First setup full path to driver name.
        //

	//printf("driverLocation = %s\n", driverLocation);
        if (!ManageDriver(DriverName1,
                          DRIVER_FUNC_INSTALL
                          )) {

            printf("Unable to install driver. \n");

            //
            // Error - remove driver.
            //

            ManageDriver(DriverName1,
                         DRIVER_FUNC_REMOVE
                         );

            return;
        }

        hDevice = CreateFile( "\\\\.\\IoctlTest",
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

        if ( hDevice == INVALID_HANDLE_VALUE ){
            printf ( "Error: CreatFile Failed : %d\n", GetLastError());
            return;
        }

    }
    printf("File \\\\.\\IoctlTest opened. hDevice = valid handle.\n");
    if ( G_test ) {
	//
	// Printing Input & Output buffer pointers and size
	//

	    printf("InputBuffer Pointer = %p, BufLength = %zd\n", InputBuffer,
                        sizeof(InputBuffer));
	    printf("OutputBuffer Pointer = %p BufLength = %zd\n", OutputBuffer,
                                sizeof(OutputBuffer));
	//
	// Performing METHOD_BUFFERED
	//

	    StringCbCopy(InputBuffer, sizeof(InputBuffer),
		    "This String is from User Application , from G_test");

    	printf("\nG_test Calling DeviceIoControl METHOD_BUFFERED:\n");

    	memset(OutputBuffer, 0, sizeof(OutputBuffer));

    	bRc = DeviceIoControl ( hDevice,
                            (DWORD) IOCTL_SIOCTL_METHOD_BUFFERED,
                            &InputBuffer,
                            (DWORD) strlen ( InputBuffer )+1,
                            &OutputBuffer,
                            sizeof( OutputBuffer),
                            &bytesReturned,
                            NULL
                            );

    	if ( !bRc )
	    {
		    printf ( "Error in DeviceIoControl : %d", GetLastError());
		    return;

	    }
    	printf("    OutBuffer (%d): %s\n", bytesReturned, OutputBuffer);

	    return;
    }

    if ( (G_disksize == 0) && (G_Cylinders == 0) && (G_TracksPerCyl == 0) && 
    	(G_SectorsPerTrk == 0) && (G_BytesPerSec == 0) &&
	    (strlen(G_DriveLetter) == 0) && !G_Umount && (G_Interval == 0) &&
	    (strlen(G_VolFilepath) == 0)) {
    	printf ( "load driver only\n");
    	return;
    }



    if (strlen(G_VolFilepath) > 0 ||  G_Umount) {

    	creatVol.DiskSize.QuadPart = G_disksize;	// 1023809024;
	    creatVol.Cylinders = (USHORT)G_Cylinders;		//124;
    	creatVol.TracksPerCyl = (USHORT)G_TracksPerCyl;	//255;
	    creatVol.SectorsPerTrk = (USHORT)G_SectorsPerTrk;	//63;
    	creatVol.BytesPerSec = (USHORT)G_BytesPerSec;	//512;

    	MultiByteToWideChar (CP_ACP,
                                0,
                                G_DriveLetter,		//input
                                -1,
                                (LPWSTR)&creatVol.DriveLetter,		//output
                                sizeof(creatVol.DriveLetter));
	printf("creatVol.DriveLetter %c%c%c by element %x\n", creatVol.DriveLetter[0],creatVol.DriveLetter[1],creatVol.DriveLetter[2], creatVol.DriveLetter[2]);
    	printf("creatVol.DriveLetter %ls\n", creatVol.DriveLetter);
    	creatVol.nonloop = (f3 + tm1->tm_min) % 256;

    	MultiByteToWideChar (CP_ACP,
                                0,
                                G_VolFilepath,		//input
                                -1,
                                creatVol.FilePath,		//output
                                sizeof(creatVol.FilePath));

    	printf("creatVol.FilePath %ls\n", creatVol.FilePath);

 
    	memset(OutputBuffer, 0, sizeof(OutputBuffer));
    	ioctl = (DWORD) IOCTL_SIOCTL_METHOD_ADD_VOLUME;
    	if ( G_Umount ) {
    	    ioctl = (DWORD) IOCTL_SIOCTL_METHOD_UMOUNT;
    	}

    	bRc = DeviceIoControl ( hDevice,
                            ioctl,
                            &creatVol,
                            sizeof(CREATE_VOL),
                            &OutputBuffer,
                            sizeof( OutputBuffer),
                            &bytesReturned,
                            NULL
                            );

	    if ( !bRc )
    	{
	        printf ( "Error in DeviceIoControl : %d", GetLastError());
	        return;
    	}
    
    } // strlen(G_VolFilepath) > 0


    if ( G_Interval > 0 ) {

	    HANDLE		e;
	    PVOL_STATS	pVolStats = NULL;
	    PVOID		buffer;
	    ULONG		bytesReturned2;
	    USHORT		*pVolCount = 0;
	    USHORT		allocVCount = 1;
	    int		i;
	    DWORD		dwSize;
	    CHAR		driveletter[3];
	    BOOLEAN		bIoCtrlSucceed;

	    e = CreateEvent(NULL, FALSE, FALSE, NULL);
	    buffer = malloc(sizeof(USHORT) + allocVCount * sizeof(VOL_STATS));
	    while ( TRUE ) {
	        bIoCtrlSucceed = FALSE;

	        while ( ! bIoCtrlSucceed ) {
    		    pVolCount = buffer;
	    	    pVolStats = (PVOL_STATS)(pVolCount + 1);
		        bRc = DeviceIoControl ( hDevice,
                                (DWORD) IOCTL_SIOCTL_METHOD_STATS_COUNT,
                                &allocVCount,
                                sizeof(allocVCount),
                                buffer,
                                sizeof(USHORT) + allocVCount * sizeof(VOL_STATS),
                                &bytesReturned2,
                                NULL
                                );
    		    if ( !bRc ) {
	    	        printf ( "Error in DeviceIoControl : %d", GetLastError());
		            return;
	    	    }
                if ( *pVolCount > allocVCount ) {
	    	        allocVCount = *pVolCount;
		            free( buffer );
		            buffer =  malloc(sizeof(USHORT) + allocVCount * sizeof(VOL_STATS));
    		        if ( buffer == NULL ) {
	        		    printf("malloc NULL\n");
			            return;
		            }
    		    }
    		    else {
	    	        bIoCtrlSucceed = TRUE;
		        }
	        } // ! bIoCtrlSucceed

	        //printf("bRC = %X volcount = %d byteReturned = %d\n", bRc, (*pVolCount), bytesReturned);
	        for (i=0; i < (*pVolCount); i++) {
		        dwSize = WideCharToMultiByte (CP_ACP,
                                    0,
                                    pVolStats->DriveLetter,
                                    sizeof(pVolStats->DriveLetter),
                                    driveletter,
                                    sizeof(driveletter),
                                    NULL,
                                    NULL);
		        printf("L=%s ",driveletter );
		        printf("R=%d W=%d\n",	
		            pVolStats->ReadCount,
		            pVolStats->WriteCount );
    		    pVolStats++;
	        }


	        WaitForSingleObject(e, G_Interval);
	    } // TRUE
	    return;

    }

    //
    // close the handle to the device.
    //

    CloseHandle ( hDevice );

    printf("File \\\\.\\IoctlTest closed\n");

    //
    // Unload the driver.  Ignore any errors.
    //

    //ManageDriver(DriverName1,
    //             driverLocation,
    //             DRIVER_FUNC_REMOVE
    //             );


}


