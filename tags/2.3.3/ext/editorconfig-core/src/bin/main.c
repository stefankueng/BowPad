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

#include "config.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <editorconfig/editorconfig.h>


static void version(FILE* stream)
{
    int     major;
    int     minor;
    int     subminor;

    editorconfig_get_version(&major, &minor, &subminor);

    fprintf(stream,"EditorConfig C Core Version %d.%d.%d%s\n",
            major, minor, subminor, editorconfig_get_version_suffix());
}

static void usage(FILE* stream, const char* command)
{
    fprintf(stream, "Usage: %s [OPTIONS] FILEPATH1 [FILEPATH2 FILEPATH3 ...]\n", command);
    fprintf(stream, "FILEPATH can be a hyphen (-) if you want to path(s) to be read from stdin.\n");

    fprintf(stream, "\n");
    fprintf(stream, "-f                 Specify conf filename other than \".editorconfig\".\n");
    fprintf(stream, "-b                 Specify version (used by devs to test compatibility).\n");
    fprintf(stream, "-h OR --help       Print this help message.\n");
    fprintf(stream, "-v OR --version    Display version information.\n");
}

int main(int argc, const char* argv[])
{
    char*                               full_filename = NULL;
    int                                 err_num;
    int                                 i;
    int                                 name_value_count;
    editorconfig_handle                 eh;
    char**                              file_paths = NULL;
    int                                 path_count; /* the count of path input*/
    /* Will be a EditorConfig file name if -f is specified on command line */
    const char*                         conf_filename = NULL;

    int                                 version_major = -1;
    int                                 version_minor = -1;
    int                                 version_subminor = -1;

    /* File names read from stdin are put in this buffer temporarily */
    char                                file_line_buffer[FILENAME_MAX + 1];

    _Bool                               f_flag = 0;
    _Bool                               b_flag = 0;

    if (argc <= 1) {
        version(stderr);
        usage(stderr, argv[0]);
        exit(1);
    }

    for (i = 1; i < argc; ++i) {

        if (b_flag) {
            char*             pos;
            int               ver;
            int               ver_pos = 0;
            char*             argvi = strdup(argv[i]);

            b_flag = 0;

            /* convert the argument -b into a version number */
            pos = strtok(argvi, ".");

            while (pos != NULL) {
                ver = atoi(pos);

                switch(ver_pos) {
                case 0:
                    version_major = ver;
                    break;
                case 1:
                    version_minor = ver;
                    break;
                case 2:
                    version_subminor = ver;
                    break;
                default:
                    fprintf(stderr, "Invalid version number: %s\n", argv[i]);
                    exit(1);
                }
                ++ ver_pos;

                pos = strtok(NULL, ".");
            }

            free(argvi);
        } else if (f_flag) {
            f_flag = 0;
            conf_filename = argv[i];
        } else if (strcmp(argv[i], "--version") == 0 ||
                strcmp(argv[i], "-v") == 0) {
            version(stdout);
            exit(0);
        } else if (strcmp(argv[i], "--help") == 0 ||
                strcmp(argv[i], "-h") == 0) {
            version(stdout);
            usage(stdout, argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-b") == 0)
            b_flag = 1;
        else if (strcmp(argv[i], "-f") == 0)
            f_flag = 1;
        else if (i < argc) {
            /* If there are other args left, regard them as file names */

            path_count = argc - i;
            file_paths = (char**) malloc(path_count * sizeof(char**));

            for (; i < argc; ++i) {
                file_paths[path_count + i - argc] = strdup(argv[i]);
                if (file_paths[path_count - argc + i] == NULL) {
                    fprintf(stderr, "Error: Unable to obtain the full path.\n");
                    exit(1);
                }
            }
        } else {
            usage(stderr, argv[0]);
            exit(1);
        }
    }

    if (!file_paths) { /* No filename is set */ 
        usage(stderr, argv[0]);
        exit(1);
    }

    /* Go through all the files in the argument list */
    for (i = 0; i < path_count; ++i) {

        int             j;

        full_filename = file_paths[i];

        /* Print the file path first, with [], if more than one file is
         * specified */
        if (path_count > 1 && strcmp(full_filename, "-"))
            printf("[%s]\n", full_filename);

        if (!strcmp(full_filename, "-")) {
            int             len;
            int             c;

            /* Read a line from stdin. If EOF encountered, continue */
            if (!fgets(file_line_buffer, FILENAME_MAX + 1, stdin)) {
                if (!feof(stdin))
                    perror("Failed to read stdin");

                free(full_filename);
                continue;
            }

            -- i;

            /* trim the trailing space characters */
            len = strlen(file_line_buffer) - 1;
            while (len >= 0 && isspace(file_line_buffer[len]))
                -- len;
            if (len < 0) /* we meet a blank line */
                continue;
            file_line_buffer[len + 1] = '\0';

            full_filename = file_line_buffer;
            while (isspace(*full_filename))
                ++ full_filename;

            full_filename = strdup(full_filename);

            printf("[%s]\n", full_filename);
        }

        /* Initialize the EditorConfig handle */
        eh = editorconfig_handle_init();

        /* Set conf file name */
        if (conf_filename)
            editorconfig_handle_set_conf_file_name(eh, conf_filename);

        /* Set the version to be compatible with */
        editorconfig_handle_set_version(eh,
                version_major, version_minor, version_subminor);

        /* parsing the editorconfig files */
        err_num = editorconfig_parse(full_filename, eh);
        free(full_filename);

        if (err_num != 0) {
            /* print error message */
            fputs(editorconfig_get_error_msg(err_num), stderr);
            if (err_num > 0)
                fprintf(stderr, "\"%s\"", editorconfig_handle_get_err_file(eh));
            fprintf(stderr, "\n");
            exit(1);
        }

        /* print the result */
        name_value_count = editorconfig_handle_get_name_value_count(eh);
        for (j = 0; j < name_value_count; ++j) {
            const char*         name;
            const char*         value;

            editorconfig_handle_get_name_value(eh, j, &name, &value);
            printf("%s=%s\n", name, value);
        }

        if (editorconfig_handle_destroy(eh) != 0) {
            fprintf(stderr, "Failed to destroy editorconfig_handle.\n");
            exit(1);
        }
    }

    free(file_paths);

    exit(0);
}

