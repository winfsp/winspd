This directory contains files that are shared across build targets.

**Headers**

- `minimal.h`: Used to eliminate dependencies on the MSVCRT libraries. Originally from the WinFsp project.

**Sources**

- `scsictl.c`: Defines functions for SCSI control. Built with the WinSpd DLL, but can also be built independently (e.g. scsitool). When building with the WinSpd DLL define an empty macro `WINSPD_DLL_INTERNAL`. When building independently define an empty macro `SPD_API`.
- `strtoint.c`: Defines `long long strtoint(const char *p, int base, int is_signed, const char **endp)`.
