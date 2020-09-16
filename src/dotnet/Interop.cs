/*
 * dotnet/Interop.cs
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
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.AccessControl;

namespace Spd.Interop
{

    [StructLayout(LayoutKind.Sequential)]
    internal struct StorageUnitParams
    {
        internal const UInt32 WriteProtected = 0x00000001;
        internal const UInt32 CacheSupported = 0x00000002;
        internal const UInt32 UnmapSupported = 0x00000004;
        internal const UInt32 EjectDisabled = 0x00000008;
        internal const int GuidSize = 16;
        internal const int ProductIdSize = 16;
        internal const int ProductRevisionLevelSize = 4;

        internal unsafe fixed Byte Guid[GuidSize];
        internal UInt64 BlockCount;
        internal UInt32 BlockLength;
        internal unsafe fixed Byte ProductId[ProductIdSize];
        internal unsafe fixed Byte ProductRevisionLevel[ProductRevisionLevelSize];
        internal Byte DeviceType;
        internal UInt32 Flags;
        internal UInt32 MaxTransferLength;
        internal unsafe fixed UInt64 Reserved[8];

        internal unsafe System.Guid GetGuid()
        {
            fixed (Byte *P = Guid)
            {
                Byte[] Bytes = new Byte[GuidSize];
                Marshal.Copy((IntPtr)P, Bytes, 0, GuidSize);
                return new System.Guid(Bytes);
            }
        }
        internal unsafe void SetGuid(System.Guid Value)
        {
            fixed (Byte *P = Guid)
            {
                Byte[] Bytes = Value.ToByteArray();
                Marshal.Copy(Bytes, 0, (IntPtr)P, 16);
            }
        }
        internal unsafe String GetProductId()
        {
            fixed (Byte *P = ProductId)
                return Marshal.PtrToStringAnsi((IntPtr)P);
        }
        internal unsafe void SetProductId(String Value)
        {
            fixed (Byte *P = ProductId)
            {
                int Size = null != Value ? Value.Length : 0;
                if (Size > ProductIdSize)
                    Size = ProductIdSize;
                for (int I = 0; Size > I; I++)
                    P[I] = (Byte)Value[I];
                for (int I = Size; ProductIdSize > I; I++)
                    P[I] = 0;
            }
        }
        internal unsafe String GetProductRevisionLevel()
        {
            fixed (Byte *P = ProductRevisionLevel)
                return Marshal.PtrToStringAnsi((IntPtr)P);
        }
        internal unsafe void SetProductRevisionLevel(String Value)
        {
            fixed (Byte *P = ProductRevisionLevel)
            {
                int Size = null != Value ? Value.Length : 0;
                if (Size > ProductRevisionLevelSize)
                    Size = ProductRevisionLevelSize;
                for (int I = 0; Size > I; I++)
                    P[I] = (Byte)Value[I];
                for (int I = Size; ProductRevisionLevelSize > I; I++)
                    P[I] = 0;
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StorageUnitStatus
    {
        public const UInt32 InformationValid = 0x00000010;
        public Byte ScsiStatus;
        public Byte SenseKey;
        public Byte ASC;
        public Byte ASCQ;
        public UInt64 Information;
        public UInt64 ReservedCSI;
        public UInt32 ReservedSKS;
        public UInt32 Flags;

        public void SetSense(Byte SenseKey, Byte ASC)
        {
            this.ScsiStatus = 2/*SCSISTAT_CHECK_CONDITION*/;
            this.SenseKey = SenseKey;
            this.ASC = ASC;
            this.Information = 0;
            this.Flags = 0;
        }
        public void SetSense(Byte SenseKey, Byte ASC, UInt64 Information)
        {
            this.ScsiStatus = 2/*SCSISTAT_CHECK_CONDITION*/;
            this.SenseKey = SenseKey;
            this.ASC = ASC;
            this.Information = Information;
            this.Flags = InformationValid;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct UnmapDescriptor
    {
        public UInt64 BlockAddress;
        public UInt32 BlockCount;
        internal UInt32 Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Partition
    {
        public Byte Type;
        public Byte Active;
        public UInt64 BlockAddress, BlockCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct StorageUnitInterface
    {
        internal struct Proto
        {
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean Read(
                IntPtr StorageUnit,
                IntPtr Buffer, UInt64 BlockAddress, UInt32 BlockCount, [MarshalAs(UnmanagedType.U1)] Boolean Flush,
                ref StorageUnitStatus Status);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean Write(
                IntPtr StorageUnit,
                IntPtr Buffer, UInt64 BlockAddress, UInt32 BlockCount, [MarshalAs(UnmanagedType.U1)] Boolean Flush,
                ref StorageUnitStatus Status);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean Flush(
                IntPtr StorageUnit,
                UInt64 BlockAddress, UInt32 BlockCount,
                ref StorageUnitStatus Status);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean Unmap(
                IntPtr StorageUnit,
                IntPtr Descriptors, UInt32 Count,
                ref StorageUnitStatus Status);
        }
        
        internal static int Size = IntPtr.Size * 16;

        internal Proto.Read Read;
        internal Proto.Write Write;
        internal Proto.Flush Flush;
        internal Proto.Unmap Unmap;
        /* BOOLEAN (*Reserved[12])(); */
    }

    [SuppressUnmanagedCodeSecurity]
    internal static class Api
    {
        internal struct Proto
        {
            /* StorageUnit */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate int SpdStorageUnitCreate(
                [MarshalAs(UnmanagedType.LPWStr)] String DeviceName,
                ref StorageUnitParams StorageUnitParams,
                IntPtr Interface,
                out IntPtr PStorageUnit);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdStorageUnitDelete(
                IntPtr StorageUnit);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdStorageUnitShutdown(
                IntPtr StorageUnit);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate int SpdStorageUnitStartDispatcher(
                IntPtr StorageUnit,
                UInt32 ThreadCount);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdStorageUnitWaitDispatcher(
                IntPtr StorageUnit);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdStorageUnitSetBufferAllocatorF(
                IntPtr StorageUnit,
                BufferAlloc BufferAlloc,
                BufferFree BufferFree);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdStorageUnitSetDebugLogF(
                IntPtr StorageUnit,
                UInt32 DebugLog);

            /* helpers */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate int SpdDefinePartitionTable(
                IntPtr Partitions, UInt32 Count, IntPtr Buffer);

            /* logging */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdPrintLog(
                IntPtr Handle,
                [MarshalAs(UnmanagedType.LPWStr)] String Format,
                [MarshalAs(UnmanagedType.LPWStr)] String Message);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdEventLog(
                UInt32 Type,
                [MarshalAs(UnmanagedType.LPWStr)] String Format,
                [MarshalAs(UnmanagedType.LPWStr)] String Message);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdServiceLog(
                UInt32 Type,
                [MarshalAs(UnmanagedType.LPWStr)] String Format,
                [MarshalAs(UnmanagedType.LPWStr)] String Message);

            /* utility */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdDebugLog(
                [MarshalAs(UnmanagedType.LPStr)] String Format,
                [MarshalAs(UnmanagedType.LPStr)] String Message);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void SpdDebugLogSetHandle(
                IntPtr Handle);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate int SpdVersion(
                out UInt32 PVersion);

            /* BufferAllocator */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate IntPtr BufferAlloc(IntPtr Size);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void BufferFree(IntPtr Pointer);
        }

        internal static Proto.SpdStorageUnitCreate SpdStorageUnitCreate;
        internal static Proto.SpdStorageUnitDelete SpdStorageUnitDelete;
        internal static Proto.SpdStorageUnitStartDispatcher SpdStorageUnitStartDispatcher;
        internal static Proto.SpdStorageUnitShutdown SpdStorageUnitShutdown;
        internal static Proto.SpdStorageUnitWaitDispatcher SpdStorageUnitWaitDispatcher;
        internal static Proto.SpdStorageUnitSetBufferAllocatorF SpdStorageUnitSetBufferAllocator;
        internal static Proto.SpdStorageUnitSetDebugLogF SpdStorageUnitSetDebugLog;
        internal static Proto.SpdDefinePartitionTable _SpdDefinePartitionTable;
        internal static Proto.SpdPrintLog SpdPrintLog;
        internal static Proto.SpdEventLog SpdEventLog;
        internal static Proto.SpdServiceLog SpdServiceLog;
        internal static Proto.SpdDebugLog SpdDebugLog;
        internal static Proto.SpdDebugLogSetHandle SpdDebugLogSetHandle;
        internal static Proto.SpdVersion SpdVersion;

        internal unsafe static Object GetUserContext(
            IntPtr NativePtr)
        {
            IntPtr UserContext = *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr));
            return IntPtr.Zero != UserContext ? GCHandle.FromIntPtr(UserContext).Target : null;
        }
        internal unsafe static void SetUserContext(
            IntPtr NativePtr,
            Object Obj)
        {
            Debug.Assert(IntPtr.Zero == *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)));
            GCHandle Handle = GCHandle.Alloc(Obj, GCHandleType.Weak);
            *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)) = (IntPtr)Handle;
        }
        internal unsafe static void DisposeUserContext(
            IntPtr NativePtr)
        {
            IntPtr UserContext = *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr));
            Debug.Assert(IntPtr.Zero != UserContext);
            if (IntPtr.Zero != UserContext)
            {
                GCHandle.FromIntPtr(UserContext).Free();
                *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)) = IntPtr.Zero;
            }
        }

        internal unsafe static UnmapDescriptor[] MakeUnmapDescriptorArray(
            IntPtr Descriptors, UInt32 Count)
        {
            UnmapDescriptor *P = (UnmapDescriptor *)Descriptors;
            UnmapDescriptor[] DescriptorArray = new UnmapDescriptor[Count];
            for (UInt32 I = 0; Count > I; I++)
            {
                DescriptorArray[I].BlockAddress = P[I].BlockAddress;
                DescriptorArray[I].BlockCount = P[I].BlockCount;
            }
            return DescriptorArray;
        }

        internal unsafe static int SpdDefinePartitionTable(Partition[] Partitions, Byte[] Buffer)
        {
            if (4 < Partitions.Length || 512 > Buffer.Length)
                return 87/*ERROR_INVALID_PARAMETER*/;

            fixed (Partition *P = Partitions)
                fixed (Byte *B = Buffer)
                    return _SpdDefinePartitionTable((IntPtr)P, (UInt32)Partitions.Length, (IntPtr)B);
        }

        internal static int SetDebugLogFile(String FileName)
        {
            IntPtr Handle;
            if ("-" == FileName)
                Handle = GetStdHandle(unchecked((UInt32)(-12))/*STD_ERROR_HANDLE*/);
            else
                Handle = CreateFileW(
                    FileName,
                    (UInt32)FileSystemRights.AppendData,
                    (UInt32)(FileShare.Read | FileShare.Write),
                    IntPtr.Zero,
                    (UInt32)FileMode.OpenOrCreate,
                    (UInt32)FileAttributes.Normal,
                    IntPtr.Zero);
            if ((IntPtr)(-1) == Handle)
                return Marshal.GetLastWin32Error();
            Api.SpdDebugLogSetHandle(Handle);
            return 0/*ERROR_SUCCESS*/;
        }

        internal static Version GetVersion()
        {
            UInt32 Version = 0;
            SpdVersion(out Version);
            return new System.Version((Int32)Version >> 16, (Int32)Version & 0xFFFF);
        }

        /* initialization */
        private static IntPtr LoadDll()
        {
            String DllName = 8 == IntPtr.Size ? "winspd-x64.dll" : "winspd-x86.dll";
            return LoadLibraryW(DllName);
        }
        private static IntPtr GetEntryPointPtr(IntPtr Module, String Name)
        {
            IntPtr Proc = GetProcAddress(Module, Name);
            if (IntPtr.Zero == Proc)
                throw new EntryPointNotFoundException("cannot get entry point " + Name);
            return Proc;
        }
        private static T GetEntryPoint<T>(IntPtr Module)
        {
            return (T)(object)Marshal.GetDelegateForFunctionPointer(
                GetEntryPointPtr(Module, typeof(T).Name), typeof(T));
        }
        private static void LoadProto(IntPtr Module)
        {
            SpdStorageUnitCreate = GetEntryPoint<Proto.SpdStorageUnitCreate>(Module);
            SpdStorageUnitDelete = GetEntryPoint<Proto.SpdStorageUnitDelete>(Module);
            SpdStorageUnitShutdown = GetEntryPoint<Proto.SpdStorageUnitShutdown>(Module);
            SpdStorageUnitStartDispatcher = GetEntryPoint<Proto.SpdStorageUnitStartDispatcher>(Module);
            SpdStorageUnitWaitDispatcher = GetEntryPoint<Proto.SpdStorageUnitWaitDispatcher>(Module);
            SpdStorageUnitSetBufferAllocator = GetEntryPoint<Proto.SpdStorageUnitSetBufferAllocatorF>(Module);
            SpdStorageUnitSetDebugLog = GetEntryPoint<Proto.SpdStorageUnitSetDebugLogF>(Module);
            _SpdDefinePartitionTable = GetEntryPoint<Proto.SpdDefinePartitionTable>(Module);
            SpdPrintLog = GetEntryPoint<Proto.SpdPrintLog>(Module);
            SpdEventLog = GetEntryPoint<Proto.SpdEventLog>(Module);
            SpdServiceLog = GetEntryPoint<Proto.SpdServiceLog>(Module);
            SpdDebugLog = GetEntryPoint<Proto.SpdDebugLog>(Module);
            SpdDebugLogSetHandle = GetEntryPoint<Proto.SpdDebugLogSetHandle>(Module);
            SpdVersion = GetEntryPoint<Proto.SpdVersion>(Module);
        }
        private static void CheckVersion()
        {
            FileVersionInfo Info;
            UInt32 Version = 0, VersionMajor, VersionMinor;
            Info = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
            SpdVersion(out Version); VersionMajor = Version >> 16; VersionMinor = Version & 0xFFFF;
            if (Info.FileMajorPart != VersionMajor || Info.FileMinorPart > VersionMinor)
                throw new TypeLoadException(String.Format(
                    "incorrect dll version (need {0}.{1}, have {2}.{3})",
                    Info.FileMajorPart, Info.FileMinorPart, VersionMajor, VersionMinor));
        }
        static Api()
        {
#if false //DEBUG
            if (Debugger.IsAttached)
                Debugger.Break();
#endif
            LoadProto(LoadDll());
            CheckVersion();
        }

        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr LoadLibraryW(
            [MarshalAs(UnmanagedType.LPWStr)] String DllName);
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr GetProcAddress(
            IntPtr hModule,
            [MarshalAs(UnmanagedType.LPStr)] String lpProcName);
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr GetStdHandle(UInt32 nStdHandle);
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr CreateFileW(
            [MarshalAs(UnmanagedType.LPWStr)] String lpFileName,
            UInt32 dwDesiredAccess,
            UInt32 dwShareMode,
            IntPtr lpSecurityAttributes,
            UInt32 dwCreationDisposition,
            UInt32 dwFlagsAndAttributes,
            IntPtr hTemplateFile);
    }

}
