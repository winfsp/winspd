/*
 * dotnet/StorageUnitHost.cs
 *
 * Copyright 2018-2020 Bill Zissimopoulos
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

using System;
using System.Runtime.InteropServices;
using System.Threading;

using Spd.Interop;

namespace Spd
{

    /// <summary>
    /// Provides a means to host a storage unit.
    /// </summary>
    public class StorageUnitHost : IDisposable
    {
        /* const */
        public const UInt32 EVENTLOG_ERROR_TYPE = 0x0001;
        public const UInt32 EVENTLOG_WARNING_TYPE = 0x0002;
        public const UInt32 EVENTLOG_INFORMATION_TYPE = 0x0004;

        /* ctor/dtor */
        /// <summary>
        /// Creates an instance of the StorageUnitHost class.
        /// </summary>
        /// <param name="StorageUnit">The storage unit to host.</param>
        public StorageUnitHost(StorageUnitBase StorageUnit)
        {
            _StorageUnit = StorageUnit;
            _ShutdownLock = new ReaderWriterLockSlim();
        }
        ~StorageUnitHost()
        {
            Dispose(false);
        }
        /// <summary>
        /// Disposes the storage unit and releases all associated resources.
        /// </summary>
        public void Dispose()
        {
            lock (this)
                Dispose(true);
            GC.SuppressFinalize(true);
        }
        protected virtual void Dispose(bool disposing)
        {
            WaitInternal(true, disposing);
        }
        private void WaitInternal(bool shutdown, bool disposing)
        {
            if (IntPtr.Zero != _StorageUnitPtr)
            {
                IntPtr StorageUnitPtr = _StorageUnitPtr;
                if (shutdown)
                    Api.SpdStorageUnitShutdown(StorageUnitPtr);
                Api.SpdStorageUnitWaitDispatcher(StorageUnitPtr);
                if (disposing)
                {
                    try
                    {
                        _StorageUnit.Stopped(this);
                    }
                    catch (Exception)
                    {
                    }
                    _ShutdownLock.EnterWriteLock();
                    try
                    {
                        _StorageUnitPtr = IntPtr.Zero;
                    }
                    finally
                    {
                        _ShutdownLock.ExitWriteLock();
                    }
                }
                else
                    _StorageUnitPtr = IntPtr.Zero;
                Api.DisposeUserContext(StorageUnitPtr);
                Api.SpdStorageUnitDelete(StorageUnitPtr);
            }
        }
 
        /* properties */
        /// <summary>
        /// Gets or sets the storage unit GUID.
        /// </summary>
        public System.Guid Guid
        {
            get { return _StorageUnitParams.GetGuid(); }
            set {  _StorageUnitParams.SetGuid(value); }
        }
        /// <summary>
        /// Gets or sets the storage unit total number of blocks.
        /// </summary>
        public UInt64 BlockCount
        {
            get { return _StorageUnitParams.BlockCount; }
            set { _StorageUnitParams.BlockCount = value; }
        }
        /// <summary>
        /// Gets or sets the storage unit block length.
        /// </summary>
        public UInt32 BlockLength
        {
            get { return _StorageUnitParams.BlockLength; }
            set { _StorageUnitParams.BlockLength = value; }
        }
        /// <summary>
        /// Gets or sets the storage unit product name/ID.
        /// </summary>
        public String ProductId
        {
            get { return _StorageUnitParams.GetProductId(); }
            set {  _StorageUnitParams.SetProductId(value); }
        }
        /// <summary>
        /// Gets or sets the storage unit product revision level (version number).
        /// </summary>
        public String ProductRevisionLevel
        {
            get { return _StorageUnitParams.GetProductRevisionLevel(); }
            set {  _StorageUnitParams.SetProductRevisionLevel(value); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the storage unit is write protected.
        /// </summary>
        public Boolean WriteProtected
        {
            get { return 0 != (_StorageUnitParams.Flags & StorageUnitParams.WriteProtected); }
            set { _StorageUnitParams.Flags |= (value ? StorageUnitParams.WriteProtected : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the storage unit supports a cache.
        /// </summary>
        public Boolean CacheSupported
        {
            get { return 0 != (_StorageUnitParams.Flags & StorageUnitParams.CacheSupported); }
            set { _StorageUnitParams.Flags |= (value ? StorageUnitParams.CacheSupported : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the storage unit supports Unmap.
        /// </summary>
        public Boolean UnmapSupported
        {
            get { return 0 != (_StorageUnitParams.Flags & StorageUnitParams.UnmapSupported); }
            set { _StorageUnitParams.Flags |= (value ? StorageUnitParams.UnmapSupported : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the storage unit has UI Eject disabled.
        /// </summary>
        public Boolean EjectDisabled
        {
            get { return 0 != (_StorageUnitParams.Flags & StorageUnitParams.EjectDisabled); }
            set { _StorageUnitParams.Flags |= (value ? StorageUnitParams.EjectDisabled : 0); }
        }
        /// <summary>
        /// Gets or sets the storage unit maximum transfer length for a single operation.
        /// </summary>
        public UInt32 MaxTransferLength
        {
            get { return _StorageUnitParams.MaxTransferLength; }
            set { _StorageUnitParams.MaxTransferLength = value; }
        }

        /* control */
        /// <summary>
        /// Start a storage unit.
        /// </summary>
        /// <param name="PipeName">
        /// A value of null adds the storage unit to the Windows storage stack.
        /// A value of "\\.\pipe\PipeName" listens on the specified pipe and
        /// allows for user mode testing (e.g. using the stgtest utility).
        /// </param>
        /// <param name="DebugLog">
        /// A value of 0 disables all debug logging.
        /// A value of -1 enables all debug logging.
        /// </param>
        /// <returns></returns>
        public int Start(
            String PipeName = null,
            UInt32 DebugLog = 0)
        {
            int Error = 0;
            try
            {
                _StorageUnit.Init(this);
            }
            catch (Exception)
            {
                Error = 574/*ERROR_UNHANDLED_EXCEPTION*/;
            }
            if (0 != Error)
                return Error;
            Error = Api.SpdStorageUnitCreate(PipeName,
                ref _StorageUnitParams, _StorageUnitInterfacePtr, out _StorageUnitPtr);
            if (0 != Error)
                return Error;
            Api.SetUserContext(_StorageUnitPtr, _StorageUnit);
            Api.SpdStorageUnitSetBufferAllocator(_StorageUnitPtr, _BufferAlloc, _BufferFree);
            Api.SpdStorageUnitSetDebugLog(_StorageUnitPtr, DebugLog);
            try
            {
                _StorageUnit.Started(this);
            }
            catch (Exception)
            {
                Error = 574/*ERROR_UNHANDLED_EXCEPTION*/;
            }
            if (0 == Error)
            {
                Error = Api.SpdStorageUnitStartDispatcher(_StorageUnitPtr, 2);
                if (0 != Error)
                    try
                    {
                        _StorageUnit.Stopped(this);
                    }
                    catch (Exception)
                    {
                    }
            }
            if (0 != Error)
            {
                Api.DisposeUserContext(_StorageUnitPtr);
                Api.SpdStorageUnitDelete(_StorageUnitPtr);
                _StorageUnitPtr = IntPtr.Zero;
            }
            return Error;
        }
        /// <summary>
        /// Shuts down the storage unit.
        /// This method can be safely called from multiple threads (after Start has been called).
        /// </summary>
        public void Shutdown()
        {
            _ShutdownLock.EnterReadLock();
            try
            {
                if (IntPtr.Zero != _StorageUnitPtr)
                    Api.SpdStorageUnitShutdown(_StorageUnitPtr);
            }
            finally
            {
                _ShutdownLock.ExitReadLock();
            }
        }
        /// <summary>
        /// Waits for the storage unit to stop.
        /// </summary>
        public void Wait()
        {
            WaitInternal(false, true);
        }
        public IntPtr StorageUnitHandle()
        {
            return _StorageUnitPtr;
        }
        /// <summary>
        /// Gets the hosted storage unit.
        /// </summary>
        /// <returns>The hosted storage unit.</returns>
        public StorageUnitBase StorageUnit()
        {
            return _StorageUnit;
        }
        public static int SpdDefinePartitionTable(Partition[] Partitions, Byte[] Buffer)
        {
            return Api.SpdDefinePartitionTable(Partitions, Buffer);
        }
        public static void Log(UInt32 Type, String Message)
        {
            Api.SpdServiceLog(Type, "%s", Message);
        }
        /// <summary>
        /// Sets the debug log file to use when debug logging is enabled.
        /// </summary>
        /// <param name="FileName">
        /// The debug log file name. A value of "-" means standard error output.
        /// </param>
        /// <returns>0 or Win32 error code.</returns>
        public static int SetDebugLogFile(String FileName)
        {
            return Api.SetDebugLogFile(FileName);
        }
        /// <summary>
        /// Return the installed version of WinSpd.
        /// </summary>
        public static Version Version()
        {
            return Api.GetVersion();
        }

        /* SPD_STORAGE_UNIT_INTERFACE */
        private static Boolean Read(
            IntPtr StorageUnitPtr,
            IntPtr Buffer, UInt64 BlockAddress, UInt32 BlockCount, Boolean Flush,
            ref StorageUnitStatus Status)
        {
            StorageUnitBase StorageUnit = (StorageUnitBase)Api.GetUserContext(StorageUnitPtr);
            try
            {
                StorageUnit.Read(_ThreadBuffer, BlockAddress, BlockCount, Flush, ref Status);
            }
            catch (Exception)
            {
                Status.SetSense(
                    StorageUnitBase.SCSI_SENSE_MEDIUM_ERROR,
                    StorageUnitBase.SCSI_ADSENSE_UNRECOVERED_ERROR);
            }
            return true;
        }
        private static Boolean Write(
            IntPtr StorageUnitPtr,
            IntPtr Buffer, UInt64 BlockAddress, UInt32 BlockCount, Boolean Flush,
            ref StorageUnitStatus Status)
        {
            StorageUnitBase StorageUnit = (StorageUnitBase)Api.GetUserContext(StorageUnitPtr);
            try
            {
                StorageUnit.Write(_ThreadBuffer, BlockAddress, BlockCount, Flush, ref Status);
            }
            catch (Exception)
            {
                Status.SetSense(
                    StorageUnitBase.SCSI_SENSE_MEDIUM_ERROR,
                    StorageUnitBase.SCSI_ADSENSE_WRITE_ERROR);
            }
            return true;
        }
        private static Boolean Flush(
            IntPtr StorageUnitPtr,
            UInt64 BlockAddress, UInt32 BlockCount,
            ref StorageUnitStatus Status)
        {
            StorageUnitBase StorageUnit = (StorageUnitBase)Api.GetUserContext(StorageUnitPtr);
            try
            {
                StorageUnit.Flush(BlockAddress, BlockCount, ref Status);
            }
            catch (Exception)
            {
                Status.SetSense(
                    StorageUnitBase.SCSI_SENSE_MEDIUM_ERROR,
                    StorageUnitBase.SCSI_ADSENSE_WRITE_ERROR);
            }
            return true;
        }
        private static Boolean Unmap(
            IntPtr StorageUnitPtr,
            IntPtr Descriptors, UInt32 Count,
            ref StorageUnitStatus Status)
        {
            StorageUnitBase StorageUnit = (StorageUnitBase)Api.GetUserContext(StorageUnitPtr);
            UnmapDescriptor[] DescriptorArray = Api.MakeUnmapDescriptorArray(Descriptors, Count);
            try
            {
                StorageUnit.Unmap(DescriptorArray, ref Status);
            }
            catch (Exception)
            {
            }
            return true;
        }

        /* BufferAllocator */
        [ThreadStatic] private static Byte[] _ThreadBuffer;
        [ThreadStatic] private static GCHandle _ThreadGCHandle;
        private static IntPtr BufferAlloc(IntPtr Size)
        {
            _ThreadBuffer = new Byte[(int)Size];
            _ThreadGCHandle = GCHandle.Alloc(_ThreadBuffer, GCHandleType.Pinned);
            return _ThreadGCHandle.AddrOfPinnedObject();
        }
        private static void BufferFree(IntPtr Pointer)
        {
            if (IntPtr.Zero != Pointer)
                _ThreadGCHandle.Free();
        }

        static StorageUnitHost()
        {
            _StorageUnitInterface.Read = Read;
            _StorageUnitInterface.Write = Write;
            _StorageUnitInterface.Flush = Flush;
            _StorageUnitInterface.Unmap = Unmap;

            _StorageUnitInterfacePtr = Marshal.AllocHGlobal(StorageUnitInterface.Size);
            Marshal.StructureToPtr(_StorageUnitInterface, _StorageUnitInterfacePtr, false);

            _BufferAlloc = BufferAlloc;
            _BufferFree = BufferFree;
        }

        private static StorageUnitInterface _StorageUnitInterface;
        private static IntPtr _StorageUnitInterfacePtr;
        private static Api.Proto.BufferAlloc _BufferAlloc;
        private static Api.Proto.BufferFree _BufferFree;
        private StorageUnitParams _StorageUnitParams;
        private StorageUnitBase _StorageUnit;
        private IntPtr _StorageUnitPtr;
        private ReaderWriterLockSlim _ShutdownLock;
    }

}
