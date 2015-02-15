#
# Copyright (c) 2011-2013 EditorConfig Team
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

option(BUILD_DOCUMENTATION
    "Use Doxygen to create the HTML and man page documentation" ON)

# Whether to install the html documentation. Only valid when
# BUILD_DOCUMENTATION is set to ON
option(INSTALL_HTML_DOC
    "Install the generated html documentation" OFF)

if(BUILD_DOCUMENTATION)

    find_package(Doxygen)

    if(DOXYGEN_FOUND)

        # Generating the major part of the doc. Although it generates
        # editorconfig.3, we discard that file
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
            COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            MAIN_DEPENDENCY
            ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
            DEPENDS
            ${PROJECT_SOURCE_DIR}/include/editorconfig/editorconfig.h
            ${PROJECT_SOURCE_DIR}/include/editorconfig/editorconfig_handle.h
            ${PROJECT_SOURCE_DIR}/logo/logo.png
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        # Generating man page of editorconfig command. Although we have other
        # .1s other than editorconfig.1, we ignore them
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile_cmd.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_cmd)

        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/man/man1/editorconfig.1
            COMMAND
            ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile_cmd"
            MAIN_DEPENDENCY
            ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile_cmd.in
            DEPENDS
            ${PROJECT_SOURCE_DIR}/include/editorconfig/editorconfig.h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

        add_custom_target(doc ALL DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
            ${CMAKE_CURRENT_BINARY_DIR}/man/man1/editorconfig.1)

        # install man pages
        set(EC_MANPAGE_DIR ${CMAKE_CURRENT_BINARY_DIR}/man)
        set(EC_MANPAGE3_DIR ${EC_MANPAGE_DIR}/man3)

        # Since we cannot avoid generating editorconfig.3 with doxygen, which
        # is the manpage of the command line interface with a manpage number 3,
        # we need to exclude it when installing man3.
        install(DIRECTORY ${EC_MANPAGE3_DIR}
            DESTINATION share/man
            PATTERN editorconfig.3 EXCLUDE
            REGEX ._include_. EXCLUDE)

        set(EC_MANPAGE1_DIR ${EC_MANPAGE_DIR}/man1)

        install(FILES
            ${EC_MANPAGE1_DIR}/editorconfig.1
            DESTINATION share/man/man1)

        # "make clean" should also clean generated docs
        set_directory_properties(PROPERTIES
            ADDITIONAL_MAKE_CLEAN_FILES "html;man")

        if(INSTALL_HTML_DOC)
            install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html"
                DESTINATION share/doc/editorconfig)
        endif(INSTALL_HTML_DOC)

    else(DOXYGEN_FOUND)
        message(WARNING
            " Doxygen is not found. Documentation will not be generated.")
    endif(DOXYGEN_FOUND)
endif(BUILD_DOCUMENTATION)
