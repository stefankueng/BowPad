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

link_directories(${CMAKE_ARCHIVE_OUTPUT_DIR})

# for gcc, BUILD_STATICALLY_LINKED_EXE_DEFAULT_VAL is OFF
if(CMAKE_COMPILER_IS_GNUCC)
    set(BUILD_STATICALLY_LINKED_EXE_DEFAULT_VAL OFF)
endif(CMAKE_COMPILER_IS_GNUCC)

option(BUILD_STATICALLY_LINKED_EXE
    "Link the standard library statically when building the executable.(Only valid for gcc)"
    ${BUILD_STATICALLY_LINKED_EXE_DEFAULT_VAL})

if(CMAKE_COMPILER_IS_GNUCC)
    if(BUILD_STATICALLY_LINKED_EXE)
        # Add -static for linker if we want a statically linked executable
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif(BUILD_STATICALLY_LINKED_EXE)
endif(CMAKE_COMPILER_IS_GNUCC)

set(editorconfig_BINSRCS
    main.c)

# targets
add_executable(editorconfig_bin ${editorconfig_BINSRCS})
target_link_libraries(editorconfig_bin editorconfig_static)
set_target_properties(editorconfig_bin PROPERTIES
    OUTPUT_NAME editorconfig
    VERSION
    ${editorconfig_VERSION_MAJOR}.${editorconfig_VERSION_MINOR}.${editorconfig_VERSION_SUBMINOR})

install(TARGETS editorconfig_bin
    RUNTIME DESTINATION bin)

