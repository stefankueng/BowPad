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

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "config.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
 * Microsoft Visual C++ and some other Windows C Compilers requires exported
 * functions in shared library to be defined with __declspec(dllexport)
 * declarator. Also, gcc >= 4 supports hiding symbols that do not need to be
 * exported.
 */
#ifdef editorconfig_shared_EXPORTS /* We are building shared lib if defined */ 
# ifdef WIN32
#  ifdef __GNUC__
#   define EDITORCONFIG_EXPORT  __attribute__ ((dllexport))
#  else /* __GNUC__ */
#   define EDITORCONFIG_EXPORT __declspec(dllexport)
#  endif /* __GNUC__ */
# else /* WIN32 */
#  if defined(__GNUC__) && __GNUC__ >= 4
#   define EDITORCONFIG_EXPORT __attribute__ ((visibility ("default")))
#   define EDITORCONFIG_LOCAL __attribute__ ((visibility ("hidden")))
#  endif /* __GNUC__ && __GNUC >= 4 */
# endif /* WIN32 */
#endif /* editorconfig_shared_EXPORTS */

/*
 * For other cases, just define EDITORCONFIG_EXPORT and EDITORCONFIG_LOCAL, to
 * make compilation successful
 */
#ifndef EDITORCONFIG_EXPORT
# define EDITORCONFIG_EXPORT
#endif
#ifndef EDITORCONFIG_LOCAL
# define EDITORCONFIG_LOCAL
#endif

/* a macro to set editorconfig_version struct */
#define SET_EDITORCONFIG_VERSION(editorconfig_ver, maj, min, submin) \
    do { \
        (editorconfig_ver)->major = (maj); \
        (editorconfig_ver)->minor = (min); \
        (editorconfig_ver)->subminor = (submin); \
    } while(0)

#endif /* !__GLOBAL_H__ */

