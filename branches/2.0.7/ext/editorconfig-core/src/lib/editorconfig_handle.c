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

#include "editorconfig_handle.h"

/*
 * See header file
 */
EDITORCONFIG_EXPORT
editorconfig_handle editorconfig_handle_init(void)
{
    editorconfig_handle     h;
    
    h = (editorconfig_handle)malloc(sizeof(struct editorconfig_handle));

    if (!h)
        return (editorconfig_handle)NULL;

    memset(h, 0, sizeof(struct editorconfig_handle));

    return h;
}

/*
 * See header file
 */
EDITORCONFIG_EXPORT
int editorconfig_handle_destroy(editorconfig_handle h)
{
    int                             i;
    struct editorconfig_handle*     eh = (struct editorconfig_handle*)h;


    if (h == NULL)
        return 0;

    /* free name_values */
    for (i = 0; i < eh->name_value_count; ++i) {
        free(eh->name_values[i].name);
        free(eh->name_values[i].value);
    }
    free(eh->name_values);

    /* free err_file */
    if (eh->err_file)
        free(eh->err_file);

    /* free eh itself */
    free(eh);

    return 0;
}

/*
 * See header file
 */
EDITORCONFIG_EXPORT
const char* editorconfig_handle_get_err_file(const editorconfig_handle h)
{
    return ((const struct editorconfig_handle*)h)->err_file;
}

/*
 * See header file
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_get_version(const editorconfig_handle h, int* major,
        int* minor, int* subminor)
{
    if (major)
        *major = ((const struct editorconfig_handle*)h)->ver.major;
    if (minor)
        *minor = ((const struct editorconfig_handle*)h)->ver.minor;
    if (subminor)
        *subminor = ((const struct editorconfig_handle*)h)->ver.subminor;
}

/*
 * See header file
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_set_version(editorconfig_handle h, int major,
        int minor, int subminor)
{
    if (major >= 0)
        ((struct editorconfig_handle*)h)->ver.major = major;

    if (minor >= 0)
        ((struct editorconfig_handle*)h)->ver.minor = minor;

    if (subminor >= 0)
        ((struct editorconfig_handle*)h)->ver.subminor = minor;
}

/*
 * See header file
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_set_conf_file_name(editorconfig_handle h,
        const char* conf_file_name)
{
    ((struct editorconfig_handle*)h)->conf_file_name = conf_file_name;
}

/*
 * See header file
 */
EDITORCONFIG_EXPORT
const char* editorconfig_handle_get_conf_file_name(const editorconfig_handle h)
{
    return ((const struct editorconfig_handle*)h)->conf_file_name;
}

EDITORCONFIG_EXPORT
void editorconfig_handle_get_name_value(const editorconfig_handle h, int n,
        const char** name, const char** value)
{
    struct editorconfig_name_value* name_value = &((
                const struct editorconfig_handle*)h)->name_values[n];

    if (name)
        *name = name_value->name;

    if (value)
        *value = name_value->value;
}

EDITORCONFIG_EXPORT
int editorconfig_handle_get_name_value_count(const editorconfig_handle h)
{
    return ((const struct editorconfig_handle*)h)->name_value_count;
}
