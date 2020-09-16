/*
 * dotnet/StorageUnitBase.cs
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

using Spd.Interop;

namespace Spd
{

    /// <summary>
    /// Provides the base class that storage units must inherit from.
    /// </summary>
    public partial class StorageUnitBase
    {
        /* operations */
        /// <summary>
        /// Occurs just before the storage unit is started.
        /// Storage units may override this method to configure the storage unit host.
        /// </summary>
        /// <param name="Host">
        /// The storage unit host that is hosting this storage unit.
        /// </param>
        public virtual void Init(Object Host)
        {
        }
        /// <summary>
        /// Occurs just after the storage unit is created,
        /// but prior to receiving any operation.
        /// </summary>
        /// <param name="Host">
        /// The storage unit host that is hosting this storage unit.
        /// </param>
        public virtual void Started(Object Host)
        {
        }
        /// <summary>
        /// Occurs just after the storage unit is stopped.
        /// No other operations will be received by this storage unit.
        /// </summary>
        /// <param name="Host">
        /// The storage unit host that is hosting this storage unit.
        /// </param>
        public virtual void Stopped(Object Host)
        {
        }
        /// <summary>
        /// Reads blocks from the storage unit.
        /// </summary>
        public virtual void Read(
            Byte[] Buffer,
            UInt64 BlockAddress,
            UInt32 BlockCount,
            Boolean Flush,
            ref StorageUnitStatus Status)
        {
        }
        /// <summary>
        /// Write blocks to the storage unit.
        /// </summary>
        public virtual void Write(
            Byte[] Buffer,
            UInt64 BlockAddress,
            UInt32 BlockCount,
            Boolean Flush,
            ref StorageUnitStatus Status)
        {
        }
        /// <summary>
        /// Flush cached blocks to the storage unit.
        /// </summary>
        public virtual void Flush(
            UInt64 BlockAddress,
            UInt32 BlockCount,
            ref StorageUnitStatus Status)
        {
        }
        /// <summary>
        /// Unmap blocks from the storage unit.
        /// </summary>
        public virtual void Unmap(
            UnmapDescriptor[] Descriptors,
            ref StorageUnitStatus Status)
        {
        }
    }

}
