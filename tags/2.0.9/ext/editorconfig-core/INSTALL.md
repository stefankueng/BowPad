Installing from a binary package
================================

Binary packages can be downloaded [here](http://sourceforge.net/projects/editorconfig/files/EditorConfig-C-Core/).

Windows users can also install EditorConfig core by [Chocolatey](http://chocolatey.org/packages/editorconfig.core).

Mac OS X users can `brew install editorconfig` with [Homebrew](http://brew.sh).
Generally Linux users can also install with [LinuxBrew](https://github.com/Homebrew/linuxbrew)
by `brew install editorconfig`.

Debian (Jessie and later): `apt-get install editorconfig`

Installing from source
======================

Before install, you need to install [cmake][]. Cmake installation instruction
could be found here: <http://www.cmake.org/cmake/help/install.html>.

Make sure cmake is in your PATH environment variable. Switch to the root
directory of editorconfig and execute the following command:

    cmake .

If successful, the project file will be generated. There are various options
could be used when generating the project file:

    -DBUILD_DOCUMENTATION=[ON|OFF]          Default: ON
    If this option is on and doxygen is found, the html documentation and
    man pages will be generated.
    e.g. cmake -DBUILD_DOCUMENTATION=OFF .

    -DBUILD_STATICALLY_LINKED_EXE=[ON|OFF]  Default: OFF
    If this option is on, the executable will be linked statically to all
    libraries. This option is currently only valid for gcc.
    e.g. cmake -DBUILD_STATICALLY_LINKED_EXE=ON .

    -DINSTALL_HTML_DOC=[ON|OFF]             Default: OFF
    If this option is on and BUILD_DOCUMENTATION is on, html documentation
    will be installed when execute "make install" or something similar.
    e.g. cmake -DINSTALL_HTML_DOC=ON .

    -DDOXYGEN_EXECUTABLE=/path/to/doxygen
    If doxygen could not be found automatically and you need to generate
    documentation, try to set this option to the path to doxygen.
    e.g. cmake -DDOXYGEN_EXECUTABLE=/opt/doxygen/bin/doxygen .

    -DMSVC_MD=[ON|OFF]                      Default: OFF
    Use /MD instead of /MT flag when compiling with Microsoft Visual C++. This
    option takes no effect when using compilers other than Microsoft Visual
    C++.
    e.g. We want to use /MD instead of /MT when compiling with MSVC.
    cmake -DMSVC_MD=ON .

On UNIX/Linux with gcc, the project file is often a Makefile, in which case you
can type "make" to compile editorconfig.  If you are using a different compiler
or platform the compilation command may differ. For example, if you generate an
NMake Makefile and you want to use Microsoft Visual C++ to build editorconfig,
then the build command would be "nmake".

After the compilation succeeds, use the following command to install (require
root or admin privilege):

    make install

This command will copy all the files you need to corresponding directories:
editorconfig libraries will be copied to PREFIX/lib and executables will be
copied to PREFIX/bin.

Note that you have to use ldconfig or change LD_LIBRARY_PATH to specify the
PREFIX/lib as one of the library searching directory on UNIX/Linux to make sure
that source files could be linked to the libraries and executables depending on
these libraries could be executed properly.

On Windows, via Developer Command Prompt for Visual Studio:

    msbuild all_build.vcxproj /p:Configuration=Release

[cmake]: http://www.cmake.org
