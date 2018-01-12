/*
 * Copyright (c) 1989, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From FreeBSD fnmatch.c 1.11
 * $Id: fnmatch.c,v 1.3 1997/08/19 02:34:30 jdp Exp $
 *
 * Copyright(C) 2011-2012 EditorConfig Team.
 * Modified by EditorConfig Team for use in EditorConfig project.
 */

#include "global.h"

/*
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "misc.h"

#include "ec_fnmatch.h"

#ifdef EOS
# undef EOS
#endif
#define EOS '\0'

static const char *rangematch(const char *, char, int);

EDITORCONFIG_LOCAL
int ec_fnmatch(const char *pattern, const char *string, int flags)
{
    const char *stringstart;
    char c, test;
    /* When multi stars presents, should_fnm_pathname will be set to 0
     * regardless of whether EC_FNM_PATHNAME is set into flags. Otherwise, the
     * value equals to flags & EC_FNM_PATHNAME */
    _Bool should_fnm_pathname = 0;

    for (stringstart = string;;)
        switch (c = *pattern++) {
        case EOS:
            if ((flags & EC_FNM_LEADING_DIR) && *string == '/')
                return (0);
            return (*string == EOS ? 0 : EC_FNM_NOMATCH);
        case '?':
            if (*string == EOS)
                return (EC_FNM_NOMATCH);
            if (*string == '.' && (flags & EC_FNM_PERIOD) &&
                    (string == stringstart ||
                     ((flags & EC_FNM_PATHNAME) && *(string - 1) == '/')))
                return (EC_FNM_NOMATCH);
            ++string;
            break;
        case '*':
            c = *pattern;

            /* see the comments above the declaration of should_fnm_pathname */
            should_fnm_pathname = (_Bool)((c != '*') &&
                    (flags & EC_FNM_PATHNAME));

            /* Collapse multiple stars. */
            while (c == '*')
                c = *++pattern;

            if (*string == '.' && (flags & EC_FNM_PERIOD) &&
                    (string == stringstart ||
                     (should_fnm_pathname && *(string - 1) == '/')))
                return (EC_FNM_NOMATCH);

            /* Optimize for pattern with * at end or before /. */
            if (c == EOS)
                if (should_fnm_pathname)
                    return ((flags & EC_FNM_LEADING_DIR) ||
                            strchr(string, '/') == NULL ?
                            0 : EC_FNM_NOMATCH);
                else
                    return (0);
            else if (c == '/' && should_fnm_pathname) {
                if ((string = strchr(string, '/')) == NULL)
                    return (EC_FNM_NOMATCH);
                break;
            }

            /* General case, use recursion. */
            while ((test = *string) != EOS) {
                if (!ec_fnmatch(pattern, string, flags & ~EC_FNM_PERIOD))
                    return (0);
                if (test == '/' && should_fnm_pathname)
                    break;
                ++string;
            }
            return (EC_FNM_NOMATCH);
        case '[':
            if (*string == EOS)
                return (EC_FNM_NOMATCH);
            if (*string == '/' && flags & EC_FNM_PATHNAME)
                return (EC_FNM_NOMATCH);
            if ((pattern =
                        rangematch(pattern, *string, flags)) == NULL)
                return (EC_FNM_NOMATCH);
            ++string;
            break;
        case '{':
            if (*string == EOS)
                return (EC_FNM_NOMATCH);

            {
                const char *next_right_brace;
                const char *next_comma;

                next_right_brace = strchr(pattern, '}');

                if (next_right_brace== NULL) { /* unclosed braces */
                    if (*string == '{') {
                        ++ string;
                        break;
                    }

                    return (EC_FNM_NOMATCH);
                }

                next_comma = strchr(pattern, ',');

                /* no comma between the two braces, including the case of {} */
                if (next_comma == NULL || next_comma > next_right_brace) {
                    if (*string == '{' && !strncmp(string + 1, pattern,
                                (size_t) (next_right_brace - pattern + 1))) {
                        string += next_right_brace - pattern + 2;
                        pattern = next_right_brace + 1;
                        break;
                    }

                    return (EC_FNM_NOMATCH);
                }
            }

            /* Here we have confirmed that it's not {} or {single} and the
             * braces are closed */
            {
                /* set to true to we could match empty */
                _Bool is_empty_matched = 0;
                _Bool is_matched = 0;  /* Do we find them matched */
                int matched_length = 0; /* the length of the matched string */
                const char *pattern1 = pattern;
                const char *comma_brace;

                for (;;) {
                    comma_brace = strpbrk(pattern, ",}");

                    /* {str0,,str1,str2}, we are on the empty one. Remember the
                     * {} case is handled in the block above */
                    if (comma_brace == pattern1)
                        is_empty_matched = 1;
                    else {
                        /* string between pattern1 and a real , or brace, i.e.
                         * not an escaped one */
                        char *this_item; 
                        int len;
                        char *p;
                        int bs_count; /* # of backslashes */

                        /* we may have backslashes before , or the brace.
                         * Handle them carefully by counting the number of
                         * backslashes */
                        for (bs_count = 0;
                                *(comma_brace - bs_count - 1) == '\\';
                                ++ bs_count)
                            ;

                        if (bs_count % 2) { /* escaped comma or brace */
                            pattern = comma_brace + 1;
                            continue;
                        }


                        this_item = strndup(pattern1,
                                (size_t) (comma_brace - pattern1));

                        /* cleanup the escaping, remember the last character
                         * cannot be an escaped backslash, since this is handled
                         * above */
                        for (p = this_item; *p; ++ p) {
                            if (*p == '\\')
                                memmove(p, p + 1, strlen(p));
                        }


                        /* matched */
                        len = strlen(this_item);
                        if (!strncmp(this_item, string, len) &&
                                ((!is_matched) || len > matched_length)) {
                            is_matched = 1;
                            matched_length = len;
                        }
                        free(this_item);
                    } 

                    pattern = comma_brace + 1;
                    pattern1 = pattern;

                    if (*comma_brace == '}') { /* end of {...} */
                        if (is_matched)
                            string += matched_length;
                        else if (!is_empty_matched)
                            return (EC_FNM_NOMATCH);
                        break;
                    }
                }
            }
            break;
        case '\\':
            if (!(flags & EC_FNM_NOESCAPE)) {
                if ((c = *pattern++) == EOS) {
                    c = '\\';
                    --pattern;
                }
            }
            /* FALLTHROUGH */
        default:
            if (c == *string)
                ;
            else if ((flags & EC_FNM_CASEFOLD) &&
                    (tolower((unsigned char)c) ==
                     tolower((unsigned char)*string)))
                ;
            else if ((flags & EC_FNM_PREFIX_DIRS) && *string == EOS &&
                    ((c == '/' && string != stringstart) ||
                     (string == stringstart+1 && *stringstart == '/')))
                return (0);
            else
                return (EC_FNM_NOMATCH);
            string++;
            break;
        }
    /* NOTREACHED */
}

    static const char *
rangematch(const char *pattern, char test, int flags)
{
    int negate, ok;
    char c, c2;

    /*
     * A bracket expression starting with an unquoted circumflex
     * character produces unspecified results (IEEE 1003.2-1992,
     * 3.13.2).  This implementation treats it like '!', for
     * consistency with the regular expression syntax.
     * J.T. Conklin (conklin@ngai.kaleida.com)
     */
    if ((negate = (*pattern == '!' || *pattern == '^')) != 0)
        ++pattern;

    if (flags & EC_FNM_CASEFOLD)
        test = tolower((unsigned char)test);

    for (ok = 0; (c = *pattern++) != ']';) {
        if (c == '\\' && !(flags & EC_FNM_NOESCAPE))
            c = *pattern++;
        if (c == EOS)
            return (NULL);

        if (flags & EC_FNM_CASEFOLD)
            c = tolower((unsigned char)c);

        if (*pattern == '-'
                && (c2 = *(pattern+1)) != EOS && c2 != ']') {
            pattern += 2;
            if (c2 == '\\' && !(flags & EC_FNM_NOESCAPE))
                c2 = *pattern++;
            if (c2 == EOS)
                return (NULL);

            if (flags & EC_FNM_CASEFOLD)
                c2 = tolower((unsigned char)c2);

            if ((unsigned char)c <= (unsigned char)test &&
                    (unsigned char)test <= (unsigned char)c2)
                ok = 1;
        } else if (c == test)
            ok = 1;
    }
    return (ok == negate ? NULL : pattern);
}
