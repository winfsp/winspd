/**
 * @file sys/driver.h
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

#ifndef WINSPD_SYS_DRIVER_H_INCLUDED
#define WINSPD_SYS_DRIVER_H_INCLUDED

#define WINSPD_SYS_INTERNAL

#define POOL_NX_OPTIN                   1
#include <ntifs.h>
#include <storport.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

/* virtual miniport functions */
HW_INITIALIZE SpdHwInitialize;
HW_STARTIO SpdHwStartIo;
VIRTUAL_HW_FIND_ADAPTER SpdHwFindAdapter;
HW_RESET_BUS SpdHwResetBus;
HW_ADAPTER_CONTROL SpdHwAdapterControl;
HW_FREE_ADAPTER_RESOURCES SpdHwFreeAdapterResources;
HW_PROCESS_SERVICE_REQUEST SpdHwProcessServiceRequest;
HW_COMPLETE_SERVICE_IRP SpdHwCompleteServiceIrp;
HW_INITIALIZE_TRACING SpdHwInitializeTracing;
HW_CLEANUP_TRACING SpdHwCleanupTracing;

#endif
