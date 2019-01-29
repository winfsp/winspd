/*
 * dotnet/StorageUnitHost.cs
 *
 * Copyright 2018-2019 Bill Zissimopoulos
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

using Spd.Interop;

namespace Spd
{

    /// <summary>
    /// Provides a means to host a storage unit.
    /// </summary>
    public class StorageUnitHost : IDisposable
    {
        /* ctor/dtor */
        /// <summary>
        /// Creates an instance of the StorageUnitHost class.
        /// </summary>
        /// <param name="StorageUnit">The storage unit to host.</param>
        public StorageUnitHost(StorageUnitBase StorageUnit)
        {
            _StorageUnit = StorageUnit;
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
            if (IntPtr.Zero != _StorageUnitPtr)
            {
                Api.SpdStorageUnitShutdownDispatcher(_StorageUnitPtr);
                Api.SpdStorageUnitWaitDispatcher(_StorageUnitPtr);
                if (disposing)
                    try
                    {
                        _StorageUnit.Stopped(this);
                    }
                    catch (Exception)
                    {
                    }
                Api.DisposeUserContext(_StorageUnitPtr);
                Api.SpdStorageUnitDelete(_StorageUnitPtr);
                _StorageUnitPtr = IntPtr.Zero;
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
        /// Stops the storage unit and releases all associated resources.
        /// </summary>
        public void Stop()
        {
            Dispose();
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
        /// <summary>
        /// Sets the debug log file to use when debug logging is enabled.
        /// </summary>
        /// <param name="FileName">
        /// The debug log file name. A value of "-" means standard error output.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public static Int32 SetDebugLogFile(String FileName)
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

#if false
        /* FSP_FILE_SYSTEM_INTERFACE */
        private static Byte[] ByteBufferNotNull = new Byte[0];
        private static Int32 ExceptionHandler(
            FileSystemBase FileSystem,
            Exception ex)
        {
            try
            {
                return FileSystem.ExceptionHandler(ex);
            }
            catch
            {
                return unchecked((Int32)0xc00000e9)/*STATUS_UNEXPECTED_IO_ERROR*/;
            }
        }
        private static Int32 GetVolumeInfo(
            IntPtr FileSystemPtr,
            out VolumeInfo VolumeInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                return FileSystem.GetVolumeInfo(
                    out VolumeInfo);
            }
            catch (Exception ex)
            {
                VolumeInfo = default(VolumeInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetVolumeLabel(
            IntPtr FileSystemPtr,
            String VolumeLabel,
            out VolumeInfo VolumeInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                return FileSystem.SetVolumeLabel(
                    VolumeLabel,
                    out VolumeInfo);
            }
            catch (Exception ex)
            {
                VolumeInfo = default(VolumeInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetSecurityByName(
            IntPtr FileSystemPtr,
            String FileName,
            IntPtr PFileAttributes/* or ReparsePointIndex */,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                UInt32 FileAttributes;
                Byte[] SecurityDescriptorBytes = null;
                Int32 Result;
                if (IntPtr.Zero != PSecurityDescriptorSize)
                    SecurityDescriptorBytes = ByteBufferNotNull;
                Result = FileSystem.GetSecurityByName(
                    FileName,
                    out FileAttributes,
                    ref SecurityDescriptorBytes);
                if (0 <= Result && 260/*STATUS_REPARSE*/ != Result)
                {
                    if (IntPtr.Zero != PFileAttributes)
                        Marshal.WriteInt32(PFileAttributes, (Int32)FileAttributes);
                    Result = Api.CopySecurityDescriptor(SecurityDescriptorBytes,
                        SecurityDescriptor, PSecurityDescriptorSize);
                }
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Create(
            IntPtr FileSystemPtr,
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            IntPtr SecurityDescriptor,
            UInt64 AllocationSize,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = FileSystem.Create(
                    FileName,
                    CreateOptions,
                    GrantedAccess,
                    FileAttributes,
                    Api.MakeSecurityDescriptor(SecurityDescriptor),
                    AllocationSize,
                    out FileNode,
                    out FileDesc,
                    out OpenFileInfo.FileInfo,
                    out NormalizedName);
                if (0 <= Result)
                {
                    if (null != NormalizedName)
                        OpenFileInfo.SetNormalizedName(NormalizedName);
                    Api.SetFullContext(ref FullContext, FileNode, FileDesc);
                }
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Open(
            IntPtr FileSystemPtr,
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = FileSystem.Open(
                    FileName,
                    CreateOptions,
                    GrantedAccess,
                    out FileNode,
                    out FileDesc,
                    out OpenFileInfo.FileInfo,
                    out NormalizedName);
                if (0 <= Result)
                {
                    if (null != NormalizedName)
                        OpenFileInfo.SetNormalizedName(NormalizedName);
                    Api.SetFullContext(ref FullContext, FileNode, FileDesc);
                }
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Overwrite(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Overwrite(
                    FileNode,
                    FileDesc,
                    FileAttributes,
                    ReplaceFileAttributes,
                    AllocationSize,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static void Cleanup(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            UInt32 Flags)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                FileSystem.Cleanup(
                    FileNode,
                    FileDesc,
                    FileName,
                    Flags);
            }
            catch (Exception ex)
            {
                ExceptionHandler(FileSystem, ex);
            }
        }
        private static void Close(
            IntPtr FileSystemPtr,
            ref FullContext FullContext)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                FileSystem.Close(
                    FileNode,
                    FileDesc);
                Api.DisposeFullContext(ref FullContext);
            }
            catch (Exception ex)
            {
                ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Read(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Read(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Offset,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Write(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 PBytesTransferred,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Write(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Offset,
                    Length,
                    WriteToEndOfFile,
                    ConstrainedIo,
                    out PBytesTransferred,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Flush(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Flush(
                    FileNode,
                    FileDesc,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetFileInfo(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.GetFileInfo(
                    FileNode,
                    FileDesc,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetBasicInfo(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetBasicInfo(
                    FileNode,
                    FileDesc,
                    FileAttributes,
                    CreationTime,
                    LastAccessTime,
                    LastWriteTime,
                    ChangeTime,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetFileSize(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetFileSize(
                    FileNode,
                    FileDesc,
                    NewSize,
                    SetAllocationSize,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Rename(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Rename(
                    FileNode,
                    FileDesc,
                    FileName,
                    NewFileName,
                    ReplaceIfExists);
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetSecurity(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Byte[] SecurityDescriptorBytes;
                Int32 Result;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                SecurityDescriptorBytes = ByteBufferNotNull;
                Result = FileSystem.GetSecurity(
                    FileNode,
                    FileDesc,
                    ref SecurityDescriptorBytes);
                if (0 <= Result)
                    Result = Api.CopySecurityDescriptor(SecurityDescriptorBytes,
                        SecurityDescriptor, PSecurityDescriptorSize);
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetSecurity(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 SecurityInformation,
            IntPtr ModificationDescriptor)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                AccessControlSections Sections;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                Sections = AccessControlSections.None;
                if (0 != (SecurityInformation & 1/*OWNER_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Owner;
                if (0 != (SecurityInformation & 2/*GROUP_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Group;
                if (0 != (SecurityInformation & 4/*DACL_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Access;
                if (0 != (SecurityInformation & 8/*SACL_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Audit;
                return FileSystem.SetSecurity(
                    FileNode,
                    FileDesc,
                    Sections,
                    Api.MakeSecurityDescriptor(ModificationDescriptor));
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 ReadDirectory(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.ReadDirectory(
                    FileNode,
                    FileDesc,
                    Pattern,
                    Marker,
                    Buffer,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 ResolveReparsePoints(
            IntPtr FileSystemPtr,
            String FileName,
            UInt32 ReparsePointIndex,
            Boolean ResolveLastPathComponent,
            out IoStatusBlock PIoStatus,
            IntPtr Buffer,
            IntPtr PSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                return FileSystem.ResolveReparsePoints(
                    FileName,
                    ReparsePointIndex,
                    ResolveLastPathComponent,
                    out PIoStatus,
                    Buffer,
                    PSize);
            }
            catch (Exception ex)
            {
                PIoStatus = default(IoStatusBlock);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetReparsePoint(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            IntPtr PSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Byte[] ReparseData;
                Object FileNode, FileDesc;
                Int32 Result;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                ReparseData = null;
                Result = FileSystem.GetReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    ref ReparseData);
                if (0 <= Result)
                    Result = Api.CopyReparsePoint(ReparseData, Buffer, PSize);
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetReparsePoint(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Api.MakeReparsePoint(Buffer, Size));
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 DeleteReparsePoint(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.DeleteReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Api.MakeReparsePoint(Buffer, Size));
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetStreamInfo(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.GetStreamInfo(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetDirInfoByName(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            out DirInfo DirInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                DirInfo = default(DirInfo);
                Int32 Result = FileSystem.GetDirInfoByName(
                    FileNode,
                    FileDesc,
                    FileName,
                    out NormalizedName,
                    out DirInfo.FileInfo);
                DirInfo.SetFileNameBuf(NormalizedName);
                return Result;
            }
            catch (Exception ex)
            {
                DirInfo = default(DirInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Control(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 ControlCode,
            IntPtr InputBuffer, UInt32 InputBufferLength,
            IntPtr OutputBuffer, UInt32 OutputBufferLength,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Control(
                    FileNode,
                    FileDesc,
                    ControlCode,
                    InputBuffer,
                    InputBufferLength,
                    OutputBuffer,
                    OutputBufferLength,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetDelete(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            Boolean DeleteFile)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetDelete(
                    FileNode,
                    FileDesc,
                    FileName,
                    DeleteFile);
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }

        static FileSystemHost()
        {
            _FileSystemInterface.GetVolumeInfo = GetVolumeInfo;
            _FileSystemInterface.SetVolumeLabel = SetVolumeLabel;
            _FileSystemInterface.GetSecurityByName = GetSecurityByName;
            _FileSystemInterface.Create = Create;
            _FileSystemInterface.Open = Open;
            _FileSystemInterface.Overwrite = Overwrite;
            _FileSystemInterface.Cleanup = Cleanup;
            _FileSystemInterface.Close = Close;
            _FileSystemInterface.Read = Read;
            _FileSystemInterface.Write = Write;
            _FileSystemInterface.Flush = Flush;
            _FileSystemInterface.GetFileInfo = GetFileInfo;
            _FileSystemInterface.SetBasicInfo = SetBasicInfo;
            _FileSystemInterface.SetFileSize = SetFileSize;
            _FileSystemInterface.Rename = Rename;
            _FileSystemInterface.GetSecurity = GetSecurity;
            _FileSystemInterface.SetSecurity = SetSecurity;
            _FileSystemInterface.ReadDirectory = ReadDirectory;
            _FileSystemInterface.ResolveReparsePoints = ResolveReparsePoints;
            _FileSystemInterface.GetReparsePoint = GetReparsePoint;
            _FileSystemInterface.SetReparsePoint = SetReparsePoint;
            _FileSystemInterface.DeleteReparsePoint = DeleteReparsePoint;
            _FileSystemInterface.GetStreamInfo = GetStreamInfo;
            _FileSystemInterface.GetDirInfoByName = GetDirInfoByName;
            _FileSystemInterface.Control = Control;
            _FileSystemInterface.SetDelete = SetDelete;

            _FileSystemInterfacePtr = Marshal.AllocHGlobal(FileSystemInterface.Size);
            Marshal.StructureToPtr(_FileSystemInterface, _FileSystemInterfacePtr, false);
        }
#endif

        private static StorageUnitInterface _StorageUnitInterface;
        private static IntPtr _StorageUnitInterfacePtr;
        private StorageUnitParams _StorageUnitParams;
        private StorageUnitBase _StorageUnit;
        private IntPtr _StorageUnitPtr;
    }

}
