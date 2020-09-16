/**
 * @file dll/mbr.c
 *
 * @copyright 2018-2020 Bill Zissimopoulos
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

#include <shared/shared.h>

/* Windows CHS defaults */
#define SPT                             63  /* sectors per track */
#define HPC                             255 /* heads per cylinder */

#define LBA_TO_CHS(lba, c, h, s)        \
    (c) = (lba) / (HPC * SPT),          \
    (h) = ((lba) / SPT) % HPC,          \
    (s) = ((lba) % SPT) + 1
#define CHS_TO_MBR(c, h, s, chs3)       \
    (chs3)[0] = (h),                    \
    (chs3)[1] = ((s) & 0x3f) | (((c) >> 2) & 0xc0),\
    (chs3)[2] = (c) & 0xff

/*
 * The WinSpd "Master Boot Record" can be used to create a partition table.
 * This is an extremely simple MBR that cannot be used to boot any OS. This
 * is not a problem because none of our storage units are bootable.
 *
 * See https://en.wikipedia.org/wiki/Master_boot_record
 * See https://thestarman.pcministry.com/asm/mbr/W7MBR.htm
 */
#pragma pack(push, mbr, 1)
struct SPD_MBR_PARTITION
{
    UINT8 Active;
    UINT8 FirstCHS[3];
    UINT8 Type;
    UINT8 LastCHS[3];
    UINT8 BlockAddress[4];
    UINT8 BlockCount[4];
};
struct SPD_MBR
{
    UINT8 Boot[440];
    UINT8 Signature[4];
    UINT8 Padding[2];
    struct SPD_MBR_PARTITION Partitions[4];
    UINT8 Magic[2];
};
static_assert(16 == sizeof(struct SPD_MBR_PARTITION),
    "16 == sizeof(struct SPD_MBR_PARTITION)");
static_assert(512 == sizeof(struct SPD_MBR),
    "512 == sizeof(struct SPD_MBR)");
#pragma pack(pop, mbr)
static struct SPD_MBR SpdMbr =
{
    .Boot =
    {
        0xCD, 0x18,                     // INT 18h; execute BASIC
        0xF4,                           // HLT
        0xEB, 0xFD,                     // JMP to HLT instruction
    },
    .Magic =
    {
        0x55, 0xAA,
    },
};

DWORD SpdDefinePartitionTable(
    SPD_PARTITION Partitions[4], ULONG Count, UINT8 Buffer0[512])
{
    struct SPD_MBR *Buffer = (PVOID)Buffer0;
    UINT64 BlockAddress, EndBlockAddress;
    UINT32 C, H, S;

    if (4 < Count)
        return ERROR_INVALID_PARAMETER;

    for (ULONG I = 0; Count > I; I++)
    {
        BlockAddress = Partitions[I].BlockAddress;
        EndBlockAddress = BlockAddress + Partitions[I].BlockCount;
        if (EndBlockAddress <= BlockAddress || EndBlockAddress > (UINT32)-1)
            return ERROR_INVALID_PARAMETER;
    }

    memcpy(Buffer, &SpdMbr, 512);
    for (ULONG I = 0; Count > I; I++)
    {
        BlockAddress = Partitions[I].BlockAddress;
        EndBlockAddress = BlockAddress + Partitions[I].BlockCount;

        Buffer->Partitions[I].Type = Partitions[I].Type;
        Buffer->Partitions[I].Active = Partitions[I].Active;

        Buffer->Partitions[I].BlockAddress[0] = (BlockAddress) & 0xff;
        Buffer->Partitions[I].BlockAddress[1] = (BlockAddress >> 8) & 0xff;
        Buffer->Partitions[I].BlockAddress[2] = (BlockAddress >> 16) & 0xff;
        Buffer->Partitions[I].BlockAddress[3] = (BlockAddress >> 24) & 0xff;
        Buffer->Partitions[I].BlockCount[0] = (Partitions[I].BlockCount) & 0xff;
        Buffer->Partitions[I].BlockCount[1] = (Partitions[I].BlockCount >> 8) & 0xff;
        Buffer->Partitions[I].BlockCount[2] = (Partitions[I].BlockCount >> 16) & 0xff;
        Buffer->Partitions[I].BlockCount[3] = (Partitions[I].BlockCount >> 24) & 0xff;

        LBA_TO_CHS((UINT32)BlockAddress, C, H, S);
        if (1023 < C)
            C = 1023, H = 254, S = 63;
        CHS_TO_MBR(C, H, S, Buffer->Partitions[I].FirstCHS);

        LBA_TO_CHS((UINT32)EndBlockAddress, C, H, S);
        if (1023 < C)
            C = 1023, H = 254, S = 63;
        CHS_TO_MBR(C, H, S, Buffer->Partitions[I].LastCHS);
    }

    return ERROR_SUCCESS;
}
