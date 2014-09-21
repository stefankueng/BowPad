[EditorConfig][]
==============

[![Build Status](https://secure.travis-ci.org/editorconfig/editorconfig-core-c.png?branch=master)](http://travis-ci.org/editorconfig/editorconfig-core-c)

EditorConfig makes it easy to maintain the correct coding style when switching
between different text editors and between different projects.  The
EditorConfig project maintains a file format and plugins for various text
editors which allow this file format to be read and used by those editors.  For
information on the file format and supported text editors, see the
[EditorConfig website][EditorConfig].


Contributing
------------

This is the README file for the *EditorConfig C Core* codebase.  This code
produces a program that accepts a filename as input and will look for
`.editorconfig` files with sections applicable to the given file, outputting
any properties found.

When developing an editor plugin for reading EditorConfig files, the
EditorConfig core code can be used to locate and parse these files. This means
the file locator, INI parser, and file globbing mechanisms can all be
maintained in one code base, resulting in less code repitition between plugins.


Installation
------------

To install the EditorConfig core from source see the [INSTALL.md][] file.

Binary installation packages for the EditorConfig core can be found on
[SourceForge downloads page][downloads].


Getting Help
------------

For help with the EditorConfig C Core code, please write to our
[mailing list][].  Bugs and feature requests should be submitted to our
[issue tracker][].

If you are writing a plugin a language that can import C libraries, you may
want to import and use the EditorConfig library directly.  If you do use the
EditorConfig core as a C library, check the [documentation][] for latest stable
version for help. The documentation for latest development version is also
available [online][dev doc].


License
-------

Unless otherwise stated, all files are distributed under the Simplified BSD
license. The inih(`src/lib/ini.c` and `src/lib/ini.h`) and fnmatch
(`src/lib/ec_fnmatch.c` and `src/lib/ec_fnmatch.h`) libraries are distributed
under the New BSD license. See LICENSE file for details.


[EditorConfig]: http://editorconfig.org "EditorConfig Homepage"
[INSTALL.md]: https://github.com/editorconfig/editorconfig-core-c/blob/master/INSTALL.md
[mailing list]: http://groups.google.com/group/editorconfig "EditorConfig mailing list"
[issue tracker]: https://github.com/editorconfig/editorconfig/issues
[documentation]: http://docs.editorconfig.org/ "EditorConfig C Core documentation"
[downloads]: https://sourceforge.net/projects/editorconfig/files/EditorConfig-C-Core/
[dev doc]: http://docs.editorconfig.org/en/master "EditorConfig C Core latest development version documentation"
