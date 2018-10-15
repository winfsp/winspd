/**
 * @file sys/adapter.c
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

#include <sys/driver.h>

ULONG SpdHwFindAdapter(
    PVOID DeviceExtension,
    PVOID HwContext,
    PVOID BusInformation,
    PVOID LowerDevice,
    PCHAR ArgumentString,
    PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    PBOOLEAN Again)
{
    return SP_RETURN_NOT_FOUND;
}

BOOLEAN SpdHwInitialize(PVOID DeviceExtension)
{
    return FALSE;
}

VOID SpdHwFreeAdapterResources(PVOID DeviceExtension)
{
}

BOOLEAN SpdHwResetBus(PVOID DeviceExtension, ULONG PathId)
{
    return FALSE;
}

SCSI_ADAPTER_CONTROL_STATUS SpdHwAdapterControl(
    PVOID DeviceExtension,
    SCSI_ADAPTER_CONTROL_TYPE ControlType,
    PVOID Parameters)
{
    return ScsiAdapterControlUnsuccessful;
}
