/**
 * @file winspd/winspd.h
 * WinSpd User Mode API.
 *
 * In order to use the WinSpd API the user mode Storage Port Driver must include
 * &lt;winspd/winspd.h&gt; and link with the winspd_x64.dll (or winspd_x86.dll) library.
 *
 * @copyright 2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinSpd.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef WINSPD_WINSPD_H_INCLUDED
#define WINSPD_WINSPD_H_INCLUDED

#define _NTSCSI_USER_MODE_
#include <windows.h>
#include <scsi.h>

#if defined(SPD_API)
#elif defined(WINSPD_DLL_INTERNAL)
#define SPD_API                         __declspec(dllexport)
#else
#define SPD_API                         __declspec(dllimport)
#endif

#include <winspd/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SCSI control */
SPD_API DWORD SpdGetDevicePath(PWSTR DeviceName, PWCHAR PathBuf, DWORD PathBufSize);
SPD_API DWORD SpdOpenDevice(PWSTR DeviceName, PHANDLE PDeviceHandle);
SPD_API DWORD SpdScsiControl(HANDLE DeviceHandle,
    DWORD Ptl, PCDB Cdb, UCHAR DataDirection, PVOID DataBuffer, PDWORD PDataLength,
    PUCHAR PScsiStatus, UCHAR SenseInfoBuffer[32]);
SPD_API DWORD SpdMemAlignAlloc(DWORD Size, DWORD AlignmentMask, PVOID *PP);
SPD_API VOID SpdMemAlignFree(PVOID P);

#ifdef __cplusplus
}
#endif

#endif
