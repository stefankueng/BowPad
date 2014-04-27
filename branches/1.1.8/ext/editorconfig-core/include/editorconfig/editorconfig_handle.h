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

/*!
 * @file editorconfig/editorconfig_handle.h
 * @brief Header file of EditorConfig handle.
 *
 * @author EditorConfig Team
 */

#ifndef __EDITORCONFIG_EDITORCONFIG_HANDLE_H__
#define __EDITORCONFIG_EDITORCONFIG_HANDLE_H__

/* When included from a user program, EDITORCONFIG_EXPORT may not be defined,
 * and we define it here*/
#ifndef EDITORCONFIG_EXPORT
# define EDITORCONFIG_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief The editorconfig handle object type
 */
typedef void*   editorconfig_handle;

/*!
 * @brief Create and intialize a default editorconfig_handle object.
 *
 * @retval NULL Failed to create the editorconfig_handle object.
 *
 * @retval non-NULL The created editorconfig_handle object is returned.
 */
EDITORCONFIG_EXPORT
editorconfig_handle editorconfig_handle_init(void);

/*!
 * @brief Destroy an editorconfig_handle object
 *
 * @param h The editorconfig_handle object needs to be destroyed.
 *
 * @retval zero The editorconfig_handle object is destroyed successfully.
 * 
 * @retval non-zero Failed to destroy the editorconfig_handle object.
 */
EDITORCONFIG_EXPORT
int editorconfig_handle_destroy(editorconfig_handle h);

/*!
 * @brief Get the err_file field of an editorconfig_handle object
 *
 * @param h The editorconfig_handle object whose err_file needs to be obtained.
 *
 * @retval NULL No error file exists.
 *
 * @retval non-NULL The pointer to the path of the file caused the parsing
 * error is returned.
 */
EDITORCONFIG_EXPORT
const char* editorconfig_handle_get_err_file(editorconfig_handle h);

/*!
 * @brief Get the version fields of an editorconfig_handle object.
 *
 * @param h The editorconfig_handle object whose version field need to be
 * obtained.
 *
 * @param major If not null, the integer pointed by major will be filled with
 * the major version field of the editorconfig_handle object.
 *
 * @param minor If not null, the integer pointed by minor will be filled with
 * the minor version field of the editorconfig_handle object.
 *
 * @param subminor If not null, the integer pointed by subminor will be filled
 * with the subminor version field of the editorconfig_handle object.
 *
 * @return None.
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_get_version(const editorconfig_handle h, int* major,
        int* minor, int* subminor);

/*!
 * @brief Set the version fields of an editorconfig_handle object.
 *
 * @param h The editorconfig_handle object whose version fields need to be set.
 *
 * @param major If not less than 0, the major version field will be set to
 * major. If this parameter is less than 0, the major version field of the
 * editorconfig_handle object will remain unchanged.
 *
 * @param minor If not less than 0, the minor version field will be set to
 * minor. If this parameter is less than 0, the minor version field of the
 * editorconfig_handle object will remain unchanged.
 *
 * @param subminor If not less than 0, the subminor version field will be set to
 * subminor. If this parameter is less than 0, the subminor version field of the
 * editorconfig_handle object will remain unchanged.
 *
 * @return None.
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_set_version(const editorconfig_handle h, int major,
        int minor, int subminor);
/*!
 * @brief Set the conf_file_name field of an editorconfig_handle object.
 *
 * @param h The editorconfig_handle object whose conf_file_name field needs to
 * be set.
 *
 * @param conf_file_name The new value of the conf_file_name field of the
 * editorconfig_handle object.
 *
 * @return None.
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_set_conf_file_name(editorconfig_handle h,
        const char* conf_file_name);

/*!
 * @brief Get the conf_file_name field of an editorconfig_handle object.
 *
 * @param h The editorconfig_handle object whose conf_file_name field needs to
 * be obtained.
 *
 * @return The value of the conf_file_name field of the editorconfig_handle
 * object.
 */
EDITORCONFIG_EXPORT
const char* editorconfig_handle_get_conf_file_name(const editorconfig_handle h);

/*!
 * @brief Get the nth name and value fields of an editorconfig_handle object.
 *
 * @param h The editorconfig_handle object whose name and value fields need to
 * be obtained.
 *
 * @param n The zero-based index of the name and value fields to be obtained.
 *
 * @param name If not null, *name will be set to point to the obtained name.
 *
 * @param value If not null, *value will be set to point to the obtained value.
 *
 * @return None.
 */
EDITORCONFIG_EXPORT
void editorconfig_handle_get_name_value(const editorconfig_handle h, int n,
        const char** name, const char** value);

/*!
 * @brief Get the count of name and value fields of an editorconfig_handle
 * object.
 *
 * @param h The editorconfig_handle object whose count of name and value fields
 * need to be obtained.
 *
 * @return the count of name and value fields of the editorconfig_handle
 * object.
 */
EDITORCONFIG_EXPORT
int editorconfig_handle_get_name_value_count(const editorconfig_handle h);

#ifdef __cplusplus
}
#endif

#endif /* !__EDITORCONFIG_EDITORCONFIG_HANDLE_H__ */

