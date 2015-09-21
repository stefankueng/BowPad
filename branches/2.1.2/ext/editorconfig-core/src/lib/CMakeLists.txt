#
# Copyright (c) 2011-2012 EditorConfig Team
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

set(editorconfig_LIBSRCS
    ec_fnmatch.c
    editorconfig.c
    editorconfig_handle.c
    ini.c
    misc.c
    )

add_library(editorconfig_shared SHARED ${editorconfig_LIBSRCS})
set_target_properties(editorconfig_shared PROPERTIES
    OUTPUT_NAME editorconfig
    SOVERSION 0
    VERSION
    ${editorconfig_VERSION_MAJOR}.${editorconfig_VERSION_MINOR}.${editorconfig_VERSION_SUBMINOR})

# We need to link Shwapi since we use PathIsRelative
if(WIN32)
    target_link_libraries(editorconfig_shared Shlwapi)
endif()

add_library(editorconfig_static STATIC ${editorconfig_LIBSRCS})
set_target_properties(editorconfig_static PROPERTIES
    OUTPUT_NAME editorconfig_static
    VERSION
    ${editorconfig_VERSION_MAJOR}.${editorconfig_VERSION_MINOR}.${editorconfig_VERSION_SUBMINOR})

# We need to link Shwapi since we use PathIsRelative
if(WIN32)
    target_link_libraries(editorconfig_static Shlwapi)
endif()

install(TARGETS editorconfig_shared editorconfig_static
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

