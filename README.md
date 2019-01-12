<h1 align="center">
    WinSpd &middot; Windows Storage Port Proxy
</h1>

<p align="center">
    <a href="https://ci.appveyor.com/project/billziss-gh/winspd">
        <img src="https://img.shields.io/appveyor/ci/billziss-gh/winspd.svg"/>
    </a>
    <br/>
    <br/>
</p>

<p align="center">
    WinSpd enables the creation of storage units ("SCSI disks") as user mode processes (i.e. without writing any kernel mode code). These storage units are added to the Windows storage stack and appear to the Windows OS as real disks that can be formatted and accessed via a file system such as NTFS.
    <br/>
    <br/>
    The capture below shows a SCSI disk created via a user mode process, which is then partitioned, formatted with NTFS, assigned a drive letter and accessed normally.
    <br/>
    <br/>
    <img src="doc/cap.gif"/>
</p>

## License

WinSpd is available under the [GPLv3](License.txt) license with a special exception for Free/Libre and Open Source Software. A commercial license is also available. Please contact Bill Zissimopoulos \<billziss at navimatics.com> for more details.
