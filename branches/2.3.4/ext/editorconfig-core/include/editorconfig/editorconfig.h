/*
 * Copyright (c) 2011-2013 EditorConfig Team
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
 * @mainpage EditorConfig C Core Documentation
 *
 * This is the documentation of EditorConfig C Core. In this documentation, you
 * could find the document of the @ref editorconfig and the document of
 * EditorConfig Core C APIs in editorconfig.h and editorconfig_handle.h.
 */

/*!
 * @page editorconfig EditorConfig Command
 *
 * @section usage Usage of the `editorconfig` command line tool
 *
 * Usage: editorconfig <em>[OPTIONS]</em> FILEPATH1 [FILEPATH2 FILEPATH3 ...]
 *
 * FILEPATH can be a hyphen (-) if you want to path(s) to be read from stdin.
 * Hyphen can also be specified with other file names. In this way, both file
 * paths from stdin and the paths specified on the command line will be used.
 * If more than one path specified on the command line, or the paths are
 * reading from stdin (even only one path is read from stdin), the output
 * format would be INI format, instead of the simple "key=value" lines.
 *
 * @htmlonly
 * <table cellpadding="5" cellspacing="5">
 *
 * <tr>
 * <td><em>-f</em></td>
 * <td>Specify conf filename other than ".editorconfig".</td>
 * </tr>
 *
 * <tr>
 * <td><em>-b</em></td>
 * <td>Specify version (used by devs to test compatibility).</td>
 * </tr>
 *
 * <tr>
 * <td><em>-h</em> OR <em>--help</em></td>
 * <td>Print this help message.</td>
 * </tr>
 *
 * <tr>
 * <td><em>--version</em></td>
 * <td>Display version information.</td>
 * </tr>
 *
 * </table>
 * @endhtmlonly
 * @manonly
 *
 * -f             Specify conf filename other than ".editorconfig".
 *
 * -b             Specify version (used by devs to test compatibility).
 *
 * -h OR --help   Print this help message.
 *
 * --version      Display version information.
 *
 * @endmanonly
 *
 * @section related Related Pages
 *
 * @ref editorconfig-format
 */

/*!
 * @page editorconfig-format EditorConfig File Format
 *
 * @section format EditorConfig File Format
 *
 * EditorConfig files use an INI format that is compatible with the format used
 * by Python ConfigParser Library, but [ and ] are allowed in the section names.
 * The section names are filepath globs, similar to the format accepted by
 * gitignore. Forward slashes (/) are used as path separators and semicolons (;)
 * or octothorpes (#) are used for comments. Comments should go individual lines.
 * EditorConfig files should be UTF-8 encoded, with either CRLF or LF line
 * separators. 
 *
 * Filename globs containing path separators (/) match filepaths in the same
 * way as the filename globs used by .gitignore files.  Backslashes (\\) are
 * not allowed as path separators.
 *
 * A semicolon character (;) starts a line comment that terminates at the end
 * of the line. Line comments and blank lines are ignored when parsing.
 * Comments may be added to the ends of non-empty lines. An octothorpe
 * character (#) may be used instead of a semicolon to denote the start of a
 * comment.
 *
 * @section file-location Filename and Location
 *
 * When a filename is given to EditorConfig a search is performed in the
 * directory of the given file and all parent directories for an EditorConfig
 * file (named ".editorconfig" by default).  All found EditorConfig files are
 * searched for sections with section names matching the given filename. The
 * search will stop if an EditorConfig file is found with the root property set
 * to true or when reaching the root filesystem directory.
 *
 * Files are read top to bottom and the most recent rules found take
 * precedence. If multiple EditorConfig files have matching sections, the rules
 * from the closer EditorConfig file are read last, so properties in closer
 * files take precedence.
 *
 * @section patterns Wildcard Patterns
 *
 * Section names in EditorConfig files are filename globs that support pattern
 * matching through Unix shell-style wildcards. These filename globs recognize
 * the following as special characters for wildcard matching:
 *
 * @htmlonly
 * <table>
 *   <tr><td><code>*</code></td><td>Matches any string of characters, except path separators (<code>/</code>)</td></tr>
 *   <tr><td><code>**</code></td><td>Matches any string of characters</td></tr>
 *   <tr><td><code>?</code></td><td>Matches any single character</td></tr>
 *   <tr><td><code>[seq]</code></td><td>Matches any single character in <i>seq</i></td></tr>
 *   <tr><td><code>[!seq]</code></td><td>Matches any single character not in <i>seq</i></td></tr>
 *   <tr><td><code>{s1,s2,s3}</code></td><td>Matches any of the strings given (separated by commas)</td></tr>
 * </table>
 * @endhtmlonly
 * @manonly
 * *            Matches any string of characters, except path separators (/)
 *
 * **           Matches any string of characters
 *
 * ?            Matches any single character
 *
 * [seq]        Matches any single character in seq
 *
 * [!seq]       Matches any single character not in seq
 *
 * {s1,s2,s3}   Matches any of the strings given (separated by commas)
 *
 * @endmanonly
 *
 * The backslash character (\) can be used to escape a character so it is not interpreted as a special character.
 *
 * @section properties Supported Properties
 *
 * EditorConfig file sections contain properties, which are name-value pairs separated by an equal sign (=). EditorConfig plugins will ignore unrecognized property names and properties with invalid values.
 *
 * Here is the list of all property names understood by EditorConfig and all valid values for these properties:
 *
 * <ul>
 * <li><strong>indent_style</strong>: set to "tab" or "space" to use hard tabs or soft tabs respectively. The values are case insensitive.</li>
 * <li><strong>indent_size</strong>: a whole number defining the number of columns used for each indentation level and the width of soft tabs (when supported). If this equals to "tab", the <strong>indent_size</strong> will be set to the tab size, which should be tab_width if <strong>tab_width</strong> is specified, or the tab size set by editor if <strong>tab_width</strong> is not specified. The values are case insensitive.</li>
 * <li><strong>tab_width</strong>: a whole number defining the number of columns used to represent a tab character. This defaults to the value of <strong>indent_size</strong> and should not usually need to be specified.</li>
 * <li><strong>end_of_line</strong>: set to "lf", "cr", or "crlf" to control how line breaks are represented. The values are case insensitive.</li>
 * <li><strong>charset</strong>: set to "latin1", "utf-8", "utf-8-bom", "utf-16be" or "utf-16le" to control the character set. Use of "utf-8-bom" is discouraged.</li>
 * <li><strong>trim_trailing_whitespace</strong>:  set to "true" to remove any whitespace characters preceeding newline characters and "false" to ensure it doesn't.</li>
 * <li><strong>insert_final_newline</strong>: set to "true" ensure file ends with a newline when saving and "false" to ensure it doesn't.</li>
 * <li><strong>root</strong>: special property that should be specified at the top of the file outside of any sections. Set to "true" to stop <code>.editorconfig</code> files search on current file. The value is case insensitive.</li>
 * </ul>
 *
 * Property names are case insensitive and all property names are lowercased when parsing.
 */

/*!
 * @file editorconfig/editorconfig.h
 * @brief Header file of EditorConfig.
 *
 * Related page: @ref editorconfig-format
 *
 * @author EditorConfig Team
 */

#ifndef __EDITORCONFIG_EDITORCONFIG_H__
#define __EDITORCONFIG_EDITORCONFIG_H__

/* When included from a user program, EDITORCONFIG_EXPORT may not be defined,
 * and we define it here*/
#ifndef EDITORCONFIG_EXPORT
# define EDITORCONFIG_EXPORT
#endif

#include <editorconfig/editorconfig_handle.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Parse editorconfig files corresponding to the file path given by
 * full_filename, and related information is input and output in h.
 *
 * An example is available at 
 * <a href=https://github.com/editorconfig/editorconfig-core/blob/master/src/bin/main.c>src/bin/main.c</a>
 * in EditorConfig C Core source code.
 *
 * @param full_filename The full path of a file that is edited by the editor
 * for which the parsing result is.
 *
 * @param h The @ref editorconfig_handle to be used and returned from this
 * function (including the parsing result). The @ref editorconfig_handle should
 * be created by editorconfig_handle_init().
 *
 * @retval 0 Everything is OK.
 *
 * @retval "Positive Integer" A parsing error occurs. The return value would be
 * the line number of parsing error. err_file obtained from h by calling
 * editorconfig_handle_get_err_file() will also be filled with the file path
 * that caused the parsing error.
 *
 * @retval "Negative Integer" Some error occured. See below for the reason of
 * the error for each return value.
 *
 * @retval EDITORCONFIG_PARSE_NOT_FULL_PATH The full_filename is not a full
 * path name.
 *
 * @retval EDITORCONFIG_PARSE_MEMORY_ERROR A memory error occurs.
 *
 * @retval EDITORCONFIG_PARSE_VERSION_TOO_NEW The required version specified in
 * @ref editorconfig_handle is greater than the current version.
 *
 */
EDITORCONFIG_EXPORT
int editorconfig_parse(const char* full_filename, editorconfig_handle h);

/*!
 * @brief Get the error message from the error number returned by
 * editorconfig_parse().
 *
 * An example is available at
 * <a href=https://github.com/editorconfig/editorconfig-core/blob/master/src/bin/main.c>src/bin/main.c</a>
 * in EditorConfig C Core source code.
 *
 * @param err_num The error number that is used to obtain the error message.
 *
 * @return The error message corresponding to err_num.
 */
EDITORCONFIG_EXPORT
const char* editorconfig_get_error_msg(int err_num);

/*!
 * editorconfig_parse() return value: the full_filename parameter of
 * editorconfig_parse() is not a full path name
 */
#define EDITORCONFIG_PARSE_NOT_FULL_PATH                (-2)
/*!
 * editorconfig_parse() return value: a memory error occurs.
 */
#define EDITORCONFIG_PARSE_MEMORY_ERROR                 (-3)
/*!
 * editorconfig_parse() return value: the required version specified in @ref
 * editorconfig_handle is greater than the current version.
 */
#define EDITORCONFIG_PARSE_VERSION_TOO_NEW              (-4)

/*!
 * @brief Get the version number of EditorConfig.
 *
 * An example is available at
 * <a href=https://github.com/editorconfig/editorconfig-core/blob/master/src/bin/main.c>src/bin/main.c</a>
 * in EditorConfig C Core source code.
 *
 * @param major If not null, the integer pointed by major will be filled with
 * the major version of EditorConfig.
 *
 * @param minor If not null, the integer pointed by minor will be filled with
 * the minor version of EditorConfig.
 *
 * @param subminor If not null, the integer pointed by subminor will be filled
 * with the subminor version of EditorConfig.
 *
 * @return None.
 */
EDITORCONFIG_EXPORT
void editorconfig_get_version(int* major, int* minor, int* subminor);

/*!
 * @brief Get the version suffix.
 *
 * @return The version suffix, such as "-development" for a development
 * version, empty string for a stable version.
 */
EDITORCONFIG_EXPORT
const char* editorconfig_get_version_suffix(void);

#ifdef __cplusplus
}
#endif

#endif /* !__EDITORCONFIG_EDITORCONFIG_H__ */

