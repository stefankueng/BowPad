/*
 * Copyright (c) 2011-2012 EditorConfig Team
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __EDITORCONFIG_HANDLE_H__
#define __EDITORCONFIG_HANDLE_H__

#include "global.h"
#include <editorconfig/editorconfig_handle.h>

/*!
 * @brief A structure containing a name and its corresponding value.
 * @author EditorConfig Team
 */
struct editorconfig_name_value
{
    /*! EditorConfig config item's name. */ 
    char*       name;
    /*! EditorConfig config item's value. */ 
    char*       value;
};

/*!
 * @brief A structure that descripts version number.
 * @author EditorConfig Team
 */
struct editorconfig_version
{
    /*! major version */
    int                     major;
    /*! minor version */
    int                     minor;
    /*! subminor version */
    int                     subminor;
};

struct editorconfig_handle
{
    /*!
     * The file name of EditorConfig conf file. If this pointer is set to NULL,
     * the file name is set to ".editorconfig" by default.
     */
    const char*                         conf_file_name;

    /*!
     * When a parsing error occured, this will point to a file that caused the
     * parsing error.
     */
    char*                               err_file;

    /*!
     * version number it should act as. Set this to 0.0.0 to act like the
     * current version.
     */
    struct editorconfig_version         ver;

    /*! Pointer to a list of editorconfig_name_value structures containing
     * names and values of the parsed result */
    struct editorconfig_name_value*     name_values;

    /*! The total count of name_values structures pointed by name_values
     * pointer */
    int                                 name_value_count;
};

#endif /* !__EDITORCONFIG_HANDLE_H__ */

