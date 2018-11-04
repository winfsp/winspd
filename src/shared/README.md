This directory contains files that are shared across build targets.

**Headers**

- `minimal.h`: Used to eliminate dependencies on the MSVCRT libraries. Originally from the WinFsp project.

**Sources**

- `ioctl.c`: Defines functions for storage unit control.
- `strtoint.c`: Defines `long long strtoint(const char *p, int base, int is_signed, const char **endp)`.
