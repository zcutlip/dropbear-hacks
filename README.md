This is a special fork of the dropbear SSH server with special hacks for running on embedded systems.

In particular is the problem that many embedded systems lack any sort of mapping between usernames, including "root", and user IDs.  As such attempting to log into an SSH server as root (or any user) results in a failure due to an uninitialized passwd struct from getpwnam().

Further is the problem that, in some cases, you want the embedded device to run your own customized shell for logins rather than the default /bin/sh.

HACKS:
-------
FAKE_ROOT: #define this to force a fake "root" uid resolution even if the target system can't resolve usernames.
ALT_SHELL: #define this to have fake root user log in with an alternate shell rather than /bin/sh
Server Master Password: specify -Y on dropbear command line to specify a "master" password to authenticate with. Useful with FAKE_ROOT when there is no root user, and therefore no root password.
Forced Home Directory: specify -H on dropbear command line to have user log in with specified home directory. Useful with FAKE_ROOT when there is no root user, and therefore no root home directory.

BUILDING:
---------
Export ALT_SHELL and FAKE_ROOT environment variables on the command line:
ALT_SHELL=/tmp/bin/sh FAKE_ROOT=1 make

To build for MIPS target architecture, ensure a mips-linux-gcc is in your PATH, and use the buildmips.sh script:
ALT_SHELL=/tmp/bin/sh FAKE_ROOT=1 ./buildmips.sh
