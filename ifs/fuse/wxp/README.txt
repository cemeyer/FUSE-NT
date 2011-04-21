These sources are intended to be checked out into the Windows DDK's folder structure to this folder: <WinDDK root>\src\filesys\fuse\

Using the build environment provided by the WinDDK, you should be able to cd to this directory and build fuse.sys by running 'nmake'.

Then, copy fuse.sys to system32/drivers/, install the registry key on the wiki (see IFS resources), and reboot your system. 