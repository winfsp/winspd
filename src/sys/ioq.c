/**
 * @file sys/ioq.c
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

#include <sys/driver.h>

NTSTATUS SpdIoqCreate(PVOID DeviceExtension, SPD_IOQ **PIoq)
{
    SPD_IOQ *Ioq;
    ULONG BucketCount = (PAGE_SIZE - sizeof *Ioq) / sizeof Ioq->ProcessBuckets[0];

    *PIoq = 0;

    Ioq = SpdAllocNonPaged(PAGE_SIZE, SpdTagIoq);
    if (0 == Ioq)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(Ioq, PAGE_SIZE);

    Ioq->DeviceExtension = DeviceExtension;
    KeInitializeSpinLock(&Ioq->SpinLock);
    SpdQeventInitialize(&Ioq->PendingEvent, 0);
    InitializeListHead(&Ioq->PendingList);
    InitializeListHead(&Ioq->ProcessList);
    Ioq->ProcessBucketCount = BucketCount;

    *PIoq = Ioq;

    return STATUS_SUCCESS;
}

VOID SpdIoqDelete(SPD_IOQ *Ioq)
{
    SpdIoqReset(Ioq, FALSE);
    SpdQeventFinalize(&Ioq->PendingEvent);
    SpdFree(Ioq, SpdTagIoq);
}

VOID SpdIoqReset(SPD_IOQ *Ioq, BOOLEAN Stop)
{
    KIRQL Irql;

    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    if (!Ioq->Stopped)
    {
        PLIST_ENTRY PendingEntry, ProcessEntry, Flink;

        PendingEntry = Ioq->PendingList.Flink;
        ProcessEntry = Ioq->ProcessList.Flink;

        InitializeListHead(&Ioq->PendingList);
        InitializeListHead(&Ioq->ProcessList);
        RtlZeroMemory(Ioq->ProcessBuckets,
            Ioq->ProcessBucketCount * sizeof Ioq->ProcessBuckets[0]);

        for (; PendingEntry != &Ioq->PendingList; PendingEntry = Flink)
        {
            /* store Flink now, because *PendingEntry becomes invalid after SpdSrbComplete */
            Flink = PendingEntry->Flink;
            SpdSrbComplete(
                Ioq->DeviceExtension,
                CONTAINING_RECORD(PendingEntry, SPD_SRB_EXTENSION, ListEntry)->Srb,
                SRB_STATUS_ABORTED);
        }
        for (; ProcessEntry != &Ioq->ProcessList; ProcessEntry = Flink)
        {
            /* store Flink now, because *ProcessEntry becomes invalid after SpdSrbComplete */
            Flink = ProcessEntry->Flink;
            SpdSrbComplete(
                Ioq->DeviceExtension,
                CONTAINING_RECORD(ProcessEntry, SPD_SRB_EXTENSION, ListEntry)->Srb,
                SRB_STATUS_ABORTED);
        }

        if (Stop)
        {
            Ioq->Stopped = TRUE;

            /* we are being stopped, permanently wake up waiters */
            SpdQeventSetNoLock(&Ioq->PendingEvent);
        }
    }

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

BOOLEAN SpdIoqStopped(SPD_IOQ *Ioq)
{
    BOOLEAN Result;
    KIRQL Irql;

    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    Result = Ioq->Stopped;

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);

    return Result;
}

NTSTATUS SpdIoqCancelSrb(SPD_IOQ *Ioq, PVOID Srb)
{
    NTSTATUS Result = STATUS_UNSUCCESSFUL;
    KIRQL Irql;

    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    if (!Ioq->Stopped)
    {
        SPD_SRB_EXTENSION *SrbExtension = SpdSrbExtension(Srb);
        ULONG Index;

        ASSERT(Srb == SrbExtension->Srb);

        Index = SpdHashMixPointer(SrbExtension) % Ioq->ProcessBucketCount;
        for (PVOID *P = &Ioq->ProcessBuckets[Index]; *P; P = &((SPD_SRB_EXTENSION *)(*P))->HashNext)
            if (*P == SrbExtension)
            {
                *P = SrbExtension->HashNext;
                SrbExtension->HashNext = 0;

                break;
            }

        RemoveEntryList(&SrbExtension->ListEntry);
        SrbExtension->ListEntry.Flink = SrbExtension->ListEntry.Blink = 0;

        SrbExtension->Srb = 0;

        SpdSrbComplete(Ioq->DeviceExtension, Srb, SRB_STATUS_ABORTED);

        Result = STATUS_SUCCESS;
    }

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);

    return Result;
}

NTSTATUS SpdIoqPostSrb(SPD_IOQ *Ioq, PVOID Srb)
{
    NTSTATUS Result = STATUS_CANCELLED;
    KIRQL Irql;

    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    if (!Ioq->Stopped)
    {
        SPD_SRB_EXTENSION *SrbExtension = SpdSrbExtension(Srb);

        ASSERT(0 == SrbExtension->Srb);
        SrbExtension->Srb = Srb;

        ASSERT(0 == SrbExtension->ListEntry.Flink && 0 == SrbExtension->ListEntry.Blink);
        InsertTailList(&Ioq->PendingList, &SrbExtension->ListEntry);

        /* queue is not empty; wake up a waiter */
        SpdQeventSetNoLock(&Ioq->PendingEvent);

        Result = STATUS_SUCCESS;
    }

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);

    return Result;
}

NTSTATUS SpdIoqStartProcessingSrb(SPD_IOQ *Ioq, PLARGE_INTEGER Timeout, PIRP CancellableIrp,
    VOID (*Prepare)(PVOID SrbExtension, PVOID Context, PVOID DataBuffer),
    PVOID Context, PVOID DataBuffer)
{
    NTSTATUS Result;
    KIRQL Irql;

    Result = SpdQeventCancellableWait(&Ioq->PendingEvent, Timeout, CancellableIrp);
    if (STATUS_TIMEOUT == Result)
        return STATUS_TIMEOUT;
    if (STATUS_CANCELLED == Result || STATUS_THREAD_IS_TERMINATING == Result)
        return STATUS_CANCELLED;
    ASSERT(STATUS_SUCCESS == Result);

    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    if (!Ioq->Stopped)
    {
        PLIST_ENTRY PendingEntry;

        PendingEntry = &Ioq->PendingList;
        if (PendingEntry->Flink != PendingEntry)
        {
            SPD_SRB_EXTENSION *SrbExtension =
                CONTAINING_RECORD(PendingEntry->Flink, SPD_SRB_EXTENSION, ListEntry);
            BOOLEAN Wake;
            ULONG Index;

            Wake = !RemoveEntryList(&SrbExtension->ListEntry);

            Prepare(SrbExtension, Context, DataBuffer);

            InsertTailList(&Ioq->ProcessList, &SrbExtension->ListEntry);
            Index = SpdHashMixPointer(SrbExtension) % Ioq->ProcessBucketCount;
#if DBG
            for (PVOID X = Ioq->ProcessBuckets[Index]; X; X = ((SPD_SRB_EXTENSION *)X)->HashNext)
                ASSERT(X != SrbExtension);
            ASSERT(0 == SrbExtension->HashNext);
#endif
            SrbExtension->HashNext = Ioq->ProcessBuckets[Index];
            Ioq->ProcessBuckets[Index] = SrbExtension;

            if (Wake)
                /* queue is not empty; wake up a waiter */
                SpdQeventSetNoLock(&Ioq->PendingEvent);

            Result = STATUS_SUCCESS;
        }
        else
            Result = STATUS_UNSUCCESSFUL;
    }
    else
    {
        /* queue is stopped; wake up a waiter */
        SpdQeventSetNoLock(&Ioq->PendingEvent);

        Result = STATUS_CANCELLED;
    }

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);

    return Result;
}

VOID SpdIoqEndProcessingSrb(SPD_IOQ *Ioq, UINT64 Hint,
    UCHAR (*Complete)(PVOID SrbExtension, PVOID Context, PVOID DataBuffer),
    PVOID Context, PVOID DataBuffer)
{
    KIRQL Irql;

    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);

    if (!Ioq->Stopped)
    {
        SPD_SRB_EXTENSION *SrbExtension = (PVOID)(UINT_PTR)Hint;
        ULONG Index;

        Index = SpdHashMixPointer(SrbExtension) % Ioq->ProcessBucketCount;
        for (PVOID *P = &Ioq->ProcessBuckets[Index]; *P; P = &((SPD_SRB_EXTENSION *)(*P))->HashNext)
            if (*P == SrbExtension)
            {
                *P = SrbExtension->HashNext;

                RemoveEntryList(&SrbExtension->ListEntry);

                UCHAR SrbStatus = Complete(SrbExtension, Context, DataBuffer);
                if (SRB_STATUS_PENDING == SrbStatus)
                {
                    /*
                     * If Complete returns PENDING we need to repost the SRB
                     * and we will also place it at the queue head, so that it
                     * gets picked up immediately after.
                     *
                     * This functionality supports splitting SRB's into chunks,
                     * which is required for I/O that exceeds our MaxTransferLength.
                     * See https://tinyurl.com/ychyv62s
                     */
                    InsertHeadList(&Ioq->PendingList, &SrbExtension->ListEntry);

                    /* queue is not empty; wake up a waiter */
                    SpdQeventSetNoLock(&Ioq->PendingEvent);
                }
                else
                    SpdSrbComplete(Ioq->DeviceExtension, SrbExtension->Srb, SrbStatus);

                break;
            }
    }

    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}
