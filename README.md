This is a special fork of the dropbear SSH server with special hacks for running on embedded systems.

In particular is the problem that many embedded systems lack any sort of mapping between usernames, including "root", and user IDs.  As such attempting to log into an SSH server as root (or any user) results in a failure due to an uninitialized passwd struct from getpwnam().

Further is the problem that, in some cases, you want the embedded device to run your own customized shell for logins rather than the default /bin/sh.