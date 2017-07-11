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

#ifndef __MISC_H__
#define __MISC_H__

#include "global.h"

#include <stddef.h>

#ifndef HAVE_STRCASECMP
# ifdef HAVE_STRICMP
#  define strcasecmp stricmp
# else /* HAVE_STRICMP */
EDITORCONFIG_LOCAL
int ec_strcasecmp(const char *s1, const char *s2);
# define strcasecmp ec_strcasecmp
# endif /* HAVE_STRICMP */
#endif /* !HAVE_STRCASECMP */
#ifndef HAVE_STRDUP
EDITORCONFIG_LOCAL
char* ec_strdup(const char *str);
# define strdup ec_strdup
#endif
#ifndef HAVE_STRNDUP
EDITORCONFIG_LOCAL
char* ec_strndup(const char* str, size_t n);
# define strndup ec_strndup
#endif
EDITORCONFIG_LOCAL
char* str_replace(char* str, char oldc, char newc);
#ifndef HAVE_STRLWR
EDITORCONFIG_LOCAL
char* ec_strlwr(char* str);
# define strlwr ec_strlwr
#endif
EDITORCONFIG_LOCAL
_Bool is_file_path_absolute(const char* path);
#endif /* __MISC_H__ */
