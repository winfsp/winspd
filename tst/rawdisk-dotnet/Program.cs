/**
 * @file Program.cs
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

using System;
using System.IO;
using System.Runtime.InteropServices;

using Spd;
using StorageUnitStatus = Spd.Interop.StorageUnitStatus;
using UnmapDescriptor = Spd.Interop.UnmapDescriptor;
using Partition = Spd.Interop.Partition;

namespace rawdisk
{
    class RawDisk : StorageUnitBase
    {
        public const UInt32 MaxTransferLength = 64 * 1024;

        public RawDisk(String RawDiskFile,
            UInt64 BlockCount, UInt32 BlockLength)
        {
            _BlockCount = BlockCount;
            _BlockLength = BlockLength;

            if (RawDiskFile.StartsWith("\\\\?\\"))
                RawDiskFile = RawDiskFile.Substring(4);

            _Stream = new FileStream(RawDiskFile,
                FileMode.OpenOrCreate, FileAccess.ReadWrite, 0, (int)MaxTransferLength, false);

            FILE_SET_SPARSE_BUFFER Sparse;
            UInt32 BytesTransferred;
            Sparse.SetSparse = true;
            _Sparse = DeviceIoControl(_Stream.SafeFileHandle.DangerousGetHandle(),
                0x900c4/*FSCTL_SET_SPARSE*/,
                ref Sparse, (UInt32)Marshal.SizeOf(Sparse),
                IntPtr.Zero, 0, out BytesTransferred, IntPtr.Zero);

            UInt64 FileSize = (UInt64)_Stream.Length;
            bool ZeroSize = 0 == FileSize;
            if (ZeroSize)
                FileSize = BlockCount * BlockLength;
            if (0 == FileSize || BlockCount * BlockLength != FileSize)
                throw new ArgumentException();
            _Stream.SetLength((long)FileSize);

            if (ZeroSize)
            {
                Partition[] Partitions = new Partition[1];
                Byte[] Buffer = new Byte[512];

                Partitions[0].Type = 7;
                Partitions[0].BlockAddress = 4096 >= BlockLength ? 4096 / BlockLength : 1;
                Partitions[0].BlockCount = BlockCount - Partitions[0].BlockAddress;
                if (0 == StorageUnitHost.SpdDefinePartitionTable(Partitions, Buffer))
                {
                    _Stream.Position = 0;
                    _Stream.Write(Buffer, 0, Buffer.Length);
                    _Stream.Flush();
                }
            }
        }

        public override void Init(Object Host0)
        {
            StorageUnitHost Host = (StorageUnitHost)Host0;
            Host.Guid = Guid.NewGuid();
            Host.BlockCount = _BlockCount;
            Host.BlockLength = _BlockLength;
            Host.MaxTransferLength = MaxTransferLength;
        }

        public override void Stopped(Object Host0)
        {
            _Stream.Dispose();
        }

        public override void Read(Byte[] Buffer, UInt64 BlockAddress, UInt32 BlockCount, Boolean Flush,
            ref StorageUnitStatus Status)
        {
            if (Flush)
                this.Flush(BlockAddress, BlockCount, ref Status);

            lock (this) /* I want pread */
            {
                _Stream.Position = (long)(BlockAddress * _BlockLength);
                _Stream.Read(Buffer, 0, (int)(BlockCount * _BlockLength));
                    /* FIX: we assume that we are reading from a file and ignore the return value */
            }
        }

        public override void Write(Byte[] Buffer, UInt64 BlockAddress, UInt32 BlockCount, Boolean Flush,
            ref StorageUnitStatus Status)
        {
            lock (this) /* I want pwrite */
            {
                _Stream.Position = (long)(BlockAddress * _BlockLength);
                _Stream.Write(Buffer, 0, (int)(BlockCount * _BlockLength));
            }

            if (Flush)
                this.Flush(BlockAddress, BlockCount, ref Status);
        }

        public override void Flush(UInt64 BlockAddress, UInt32 BlockCount,
            ref StorageUnitStatus Status)
        {
            _Stream.Flush();
        }

        public override void Unmap(UnmapDescriptor[] Descriptors,
            ref StorageUnitStatus Status)
        {
            FILE_ZERO_DATA_INFORMATION Zero;
            UInt32 BytesTransferred;
            for (int I = 0; Descriptors.Length > I; I++)
            {
                Boolean SetZero = false;

                if (_Sparse)
                {
                    Zero.FileOffset = (long)(Descriptors[I].BlockAddress * _BlockLength);
                    Zero.BeyondFinalZero = (long)((Descriptors[I].BlockAddress + Descriptors[I].BlockCount) *
                        _BlockLength);
                    SetZero = DeviceIoControl(_Stream.SafeFileHandle.DangerousGetHandle(),
                        0x980c8/*FSCTL_SET_ZERO_DATA*/,
                        ref Zero, (UInt32)Marshal.SizeOf(Zero),
                        IntPtr.Zero, 0, out BytesTransferred, IntPtr.Zero);
                }

                if (!SetZero)
                {
                    lock (this) /* I want pwrite */
                    {
                        _Stream.Position = (long)(Descriptors[I].BlockAddress * _BlockLength);

                        int TotalLength = (int)(Descriptors[I].BlockCount * _BlockLength);
                        Byte[] Buffer = new Byte[Math.Min(64 * 1024, TotalLength)];

                        while (0 < TotalLength)
                        {
                            _Stream.Write(Buffer, 0, Buffer.Length);
                            TotalLength -= Buffer.Length;
                        }
                    }
                }
            }
        }

        private UInt64 _BlockCount;
        private UInt32 _BlockLength;
        private FileStream _Stream;
        private Boolean _Sparse;

        /* interop */
        [StructLayout(LayoutKind.Sequential)]
        private struct FILE_SET_SPARSE_BUFFER
        {
            public Boolean SetSparse;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct FILE_ZERO_DATA_INFORMATION
        {
            public Int64 FileOffset;
            public Int64 BeyondFinalZero;
        }
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static extern Boolean DeviceIoControl(
            IntPtr hDevice,
            UInt32 dwIoControlCode,
            ref FILE_SET_SPARSE_BUFFER lpInBuffer,
            UInt32 nInBufferSize,
            IntPtr lpOutBuffer,
            UInt32 nOutBufferSize,
            out UInt32 lpBytesReturned,
            IntPtr Overlapped);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static extern Boolean DeviceIoControl(
            IntPtr hDevice,
            UInt32 dwIoControlCode,
            ref FILE_ZERO_DATA_INFORMATION lpInBuffer,
            UInt32 nInBufferSize,
            IntPtr lpOutBuffer,
            UInt32 nOutBufferSize,
            out UInt32 lpBytesReturned,
            IntPtr Overlapped);
    }

    class Program
    {
        private class CommandLineUsageException : Exception
        {
            public CommandLineUsageException(String Message = null) : base(Message)
            {
                HasMessage = null != Message;
            }

            public bool HasMessage;
        }

        private const String PROGNAME = "rawdisk-dotnet";

        private static void argtos(String[] Args, ref int I, ref String V)
        {
            if (Args.Length > ++I)
                V = Args[I];
            else
                throw new CommandLineUsageException();
        }
        private static void argtol(String[] Args, ref int I, ref UInt32 V)
        {
            Int32 R;
            if (Args.Length > ++I)
                V = Int32.TryParse(Args[I], out R) ? (UInt32)R : V;
            else
                throw new CommandLineUsageException();
        }

        static void Main(string[] Args)
        {
            try
            {
                String RawDiskFile = null;
                UInt32 BlockCount = 1024 * 1024;
                UInt32 BlockLength = 512;
                String ProductId = "RawDisk-dotnet";
                String ProductRevision = "1.0";
                UInt32 WriteAllowed = 1;
                UInt32 CacheSupported = 1;
                UInt32 UnmapSupported = 1;
                UInt32 DebugFlags = 0;
                String DebugLogFile = null;
                String PipeName = null;
                StorageUnitHost Host = null;
                RawDisk RawDisk = null;
                int I;

                for (I = 0; Args.Length > I; I++)
                {
                    String Arg = Args[I];
                    if ('-' != Arg[0])
                        break;
                    switch (Arg[1])
                    {
                    case '?':
                        throw new CommandLineUsageException();
                    case 'c':
                        argtol(Args, ref I, ref BlockCount);
                        break;
                    case 'C':
                        argtol(Args, ref I, ref CacheSupported);
                        break;
                    case 'd':
                        argtol(Args, ref I, ref DebugFlags);
                        break;
                    case 'D':
                        argtos(Args, ref I, ref DebugLogFile);
                        break;
                    case 'f':
                        argtos(Args, ref I, ref RawDiskFile);
                        break;
                    case 'i':
                        argtos(Args, ref I, ref ProductId);
                        break;
                    case 'l':
                        argtol(Args, ref I, ref BlockLength);
                        break;
                    case 'p':
                        argtos(Args, ref I, ref PipeName);
                        break;
                    case 'r':
                        argtos(Args, ref I, ref ProductRevision);
                        break;
                    case 'U':
                        argtol(Args, ref I, ref UnmapSupported);
                        break;
                    case 'W':
                        argtol(Args, ref I, ref WriteAllowed);
                        break;
                    default:
                        throw new CommandLineUsageException();
                    }
                }

                if (Args.Length > I)
                    throw new CommandLineUsageException();

                if (null == RawDiskFile)
                    throw new CommandLineUsageException();

                if (null != DebugLogFile)
                    if (0 != StorageUnitHost.SetDebugLogFile(DebugLogFile))
                        throw new CommandLineUsageException("cannot open debug log file");

                Host = new StorageUnitHost(RawDisk = new RawDisk(RawDiskFile, BlockCount, BlockLength));
                Host.ProductId = ProductId;
                Host.ProductRevisionLevel = ProductRevision;
                Host.WriteProtected = 0 == WriteAllowed;
                Host.CacheSupported = 0 != CacheSupported;
                Host.UnmapSupported = 0 != UnmapSupported;
                if (0 != Host.Start(PipeName, DebugFlags))
                    throw new IOException("cannot start storage unit");

                StorageUnitHost.Log(StorageUnitHost.EVENTLOG_INFORMATION_TYPE,
                    String.Format(
                        "{0} -f {1} -c {2} -l {3} -i {4} -r {5} -W {6} -C {7} -U {8}{9}{10}",
                        PROGNAME,
                        RawDiskFile,
                        BlockCount, BlockLength, ProductId, ProductRevision,
                        0 != WriteAllowed ? 1 : 0,
                        0 != CacheSupported ? 1 : 0,
                        0 != UnmapSupported ? 1 : 0,
                        null != PipeName ? " -p " : "",
                        null != PipeName ? PipeName : ""));

                Console.CancelKeyPress +=
                    delegate (Object Sender, ConsoleCancelEventArgs Event)
                    {
                        Host.Shutdown();
                        Event.Cancel = true;
                    };
                Host.Wait();
            }
            catch (CommandLineUsageException ex)
            {
                StorageUnitHost.Log(StorageUnitHost.EVENTLOG_ERROR_TYPE,
                    String.Format(
                        "{0}" +
                        "usage: {1} OPTIONS\n" +
                        "\n" +
                        "options:\n" +
                        "    -f RawDiskFile                      Storage unit data file\n" +
                        "    -c BlockCount                       Storage unit size in blocks\n" +
                        "    -l BlockLength                      Storage unit block length\n" +
                        "    -i ProductId                        1-16 chars\n" +
                        "    -r ProductRevision                  1-4 chars\n" +
                        "    -W 0|1                              Disable/enable writes (deflt: enable)\n" +
                        "    -C 0|1                              Disable/enable cache (deflt: enable)\n" +
                        "    -U 0|1                              Disable/enable unmap (deflt: enable)\n" +
                        "    -d -1                               Debug flags\n" +
                        "    -D DebugLogFile                     Debug log file; - for stderr\n" +
                        "    -p \\\\.\\pipe\\PipeName                Listen on pipe; omit to use driver\n",
                        ex.HasMessage ? ex.Message + "\n" : "",
                        PROGNAME));
                Environment.ExitCode = 87/*ERROR_INVALID_PARAMETER*/;
            }
            catch (Exception ex)
            {
                if (ex is TypeInitializationException && null != ex.InnerException)
                    ex = ex.InnerException;

                StorageUnitHost.Log(StorageUnitHost.EVENTLOG_ERROR_TYPE,
                    ex.Message);

                int hr = Marshal.GetHRForException(ex);
                Environment.ExitCode = Marshal.GetHRForException(ex);
                if ((hr & 0xffff0000) == 0x80070000)
                    Environment.ExitCode = hr & 0xffff;
                else
                    Environment.ExitCode = 574/*ERROR_UNHANDLED_EXCEPTION*/;
            }
        }
    }
}
