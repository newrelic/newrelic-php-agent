/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_STRINGS_HDR
#define UTIL_STRINGS_HDR

/*
 * All code in the PHP Agent or Daemon should NOT call the string functions
 * from <string.h>.  Instead, if you want to call foo, call nr_foo which is
 * defined here.  These functions are more robust in the presence
 * of null pointers, or in the presence of special-valued pointers based on
 * small integers.
 */

#include "nr_axiom.h"

#include <stdbool.h>
#include <string.h>

#include "util_object.h"
#include "util_memory.h"

/*
 * Purpose : Convert a string to lower case, following USASCII rules, returning
 * a newly allocated string.
 *
 * Params  : 1. The string to downcase.
 *
 * Returns : The newly created string.  The caller must arrange to free the
 * string.
 */
extern char* nr_string_to_lowercase(const char* str);

/*
 * Purpose : Wrap asprintf to increase convenience and safety: asprintf behavior
 *           is not in POSIX, and the return string may not be defined in the
 *           case of an error.
 */
extern char* nr_formatf(const char* fmt, ...) NRPRINTFMT(1);

/*
 * This function splits the incoming string into multiple strings based on a
 * set of one or more single character delimiters. It returns an array with
 * the individual strings. If the use_empty flag is set, it will include
 * empty strings, otherwise it strips them out of the return array and they are
 * ignored. All strings in the return array will have both the leading and
 * trailing whitespace removed from them.
 */
extern nrobj_t* nr_strsplit(const char* orig, const char* delim, int use_empty);

extern int nr_strnidx_impl(const char* str, const char* needle, int str_len);

/*
 * PHP often uses strings that either contain NUL characters or are not NUL
 * terminated, but their exact length is always known. This function copies
 * exactly the number of bytes specified, and the destination buffer must be
 * at least len+1 bytes long, to make space for the terminating NUL.
 *
 * Returns NULL or a pointer to the terminating NUL in the destination string
 * for easy concatenation.
 */
extern char* nr_strxcpy(char* dest, const char* src, int len);

/*
 * Null-safe version of strlen().
 */
static inline int nr_strlen(const char* str) {
  return (nrlikely(str)) ? strlen(str) : 0;
}

/*
 * Null-safe check for an empty string, "".  Returns 1 if str is NULL or
 * the first character of str is '\0'.  Returns 0 otherwise.
 */
static inline int nr_strempty(const char* str) {
  return (NULL == str || '\0' == str[0]);
}

/*
 * Should never return anything > maxlen. Longest possible string is maxlen.
 */
extern int nr_strnlen(const char* str, int maxlen) __attribute__((pure));

/*
 * Null-safe version of strcpy(). Returns pointer to terminating
 * NUL character, NOT the start of the buffer the way the normal strcpy() does.
 */
extern char* nr_strcpy(char* dest, const char* src);

/*
 * Unlike strxcpy() above, this function requires a NUL terminated string as
 * its source, and copies it into dest. The LEN argument must be big enough
 * to hold the string AND it's terminating NUL. Most commonly, the LEN param
 * is sizeof (dest). Stated slightly differently, this function will copy up
 * to a maximum of LEN-1 characters, and the resulting string will ALWAYS
 * be NUL terminated. Returns a pointer to the terminating NUL or NULL on
 * error.
 */
extern char* nr_strlcpy(char* dest, const char* src, int len);

/*
 * Null-safe version of strcat.
 * Returns a pointer to the terminating NUL or NULL on error.
 */
extern char* nr_strcat(char* dest, const char* src);

/*
 * Null-safe version of strncat;
 * Returns a pointer to the terminating NUL or NULL on error. Will copy at
 * most LEN bytes out of the source.
 */
extern char* nr_strncat(char* dest, const char* src, int len);

static inline int nr_strcmp(const char* s1, const char* s2) {
  if (nrlikely(s1 && s2)) {
    return strcmp(s1, s2);
  }
  return s1 ? 1 : (s2 ? -1 : 0);
}

static inline int nr_stricmp(const char* s1, const char* s2) {
  if (nrlikely(s1 && s2)) {
    return strcasecmp(s1, s2);
  }
  return s1 ? 1 : (s2 ? -1 : 0);
}

static inline int nr_strncmp(const char* s1, const char* s2, int n) {
  if (nrlikely(s1 && s2 && (n > 0))) {
    return strncmp(s1, s2, n);
  } else if ((0 == n) || ((NULL == s1) && (NULL == s2))) {
    return 0;
  }
  return s1 ? 1 : -1;
}

static inline int nr_strnicmp(const char* s1, const char* s2, int n) {
  if (nrlikely(s1 && s2 && (n > 0))) {
    return strncasecmp(s1, s2, n);
  } else if ((0 == n) || ((NULL == s1) && (NULL == s2))) {
    return 0;
  }
  return s1 ? 1 : -1;
}

/*
 * Determines whether two strings are equal. Returns non-zero if s1 equals s2;
 * otherwise, zero. Note that nr_strieq is case-insensitive.
 */
static inline int nr_streq(const char* s1, const char* s2) {
  return 0 == nr_strcmp(s1, s2);
}

static inline int nr_strieq(const char* s1, const char* s2) {
  return 0 == nr_stricmp(s1, s2);
}

/*
 * Null-safe versions of strchr/strrchr.
 */

static inline char* nr_strchr(const char* str, int c) {
  return nrlikely(str) ? strchr(str, c) : NULL;
}

static inline char* nr_strrchr(const char* str, int c) {
  return nrlikely(str) ? strrchr(str, c) : NULL;
}

/*
 * Null-safe version of strstr.
 *
 * Params  : 1. The string to be searched (the haystack).
 *           2. The substring being searched for (the needle).
 */
static inline char* nr_strstr(const char* str, const char* needle) {
  return nrlikely(str && needle) ? strstr(str, needle) : NULL;
}

/*
 * Purpose : Find a substring within a null terminated string.
 *
 * Params  : 1. The string to be searched.
 *           2. The substring being searched for.
 *
 * Returns : The index of the match, or -1 if no match is found.
 *
 * Notes   : nr_strcaseidx is case insensitive.
 */
extern int nr_stridx(const char* str, const char* needle);
extern int nr_strcaseidx(const char* str, const char* needle);

/*
 * Purpose : Find a substring within a limited length string.
 *
 * Params  : 1. The string to be searched.
 *           2. The substring being searched for.
 *           3. The maximum number of characters within 'str' to search.
 *
 * Returns : The index of the match, or -1 if no match is found.
 *
 * Notes   : Searching will end before this limit if '\0' is reached. The 'case'
 *           versions are case-insensitive.  The 'last_match' version will find
 *           the last substring match instead of the first match.
 */
extern int nr_strnidx(const char* str, const char* needle, int str_len);
extern int nr_strncaseidx(const char* str, const char* needle, int len);
extern int nr_strncaseidx_last_match(const char* str,
                                     const char* needle,
                                     int len);

/*
 * strspn / strcspn replacements. For the base functions we just verify the
 * arguments to ensure they are safe. However, the standard C library doesn't
 * provide any form of sized strspn/strcspn functions where you can specify
 * the length of either the needle (s2) or the haystack (s1).
 * So we provide our own function that does just that.
 */

static inline int nr_strspn(const char* s1, const char* s2) {
  return nrlikely(s1 && s2) ? strspn(s1, s2) : 0;
}

static inline int nr_strcspn(const char* s1, const char* s2) {
  return nrlikely(s1 && s2) ? strcspn(s1, s2) : 0;
}

extern int nr_strnspn(const char* s1, int s1len, const char* s2, int s2len);
extern int nr_strncspn(const char* s1, int s1len, const char* s2, int s2len);

/*
 * Purpose : Count the number of instances of a particular character in a
 *           string.
 *
 * Params  : 1. The string to examine.
 *           2. The character to look for.
 *
 * Returns : The number of instances.
 */
extern int nr_str_char_count(const char* s, char c);

/*
 * Purpose : Append a string to the end of another string separated by a
 * delimiter.
 *
 * Params  : 1. The destination string.
 *           2. The source string.
 *           3. The delimiter to separate the strings; NULL treated as empty
 * string.
 *
 * Returns : A newly allocated string containing both.
 */
extern char* nr_str_append(char* dest, const char* src, const char* delimiter);

/*
 * Purpose : Test for an alphanumeric character using the "C" locale. In the "C"
 *           locale, only the following are alphanumeric characters.
 *
 *           0 1 2 3 4 5 5 6 7 8 9
 *           A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
 *           a b c d e f g h i j k l m n o p q r s t u v w x y z
 *
 *           Unlike isalnum, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is an alphanumeric character; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isalnum.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isalnum(int c) {
  return (('0' <= c) && (c <= '9')) || (('A' <= c) && (c <= 'Z'))
         || (('a' <= c) && (c <= 'z'));
}

/*
 * Purpose : Test for an alphabetic character using the "C" locale. In the "C"
 *           locale, only the following are alphabetic characters.
 *
 *           A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
 *           a b c d e f g h i j k l m n o p q r s t u v w x y z
 *
 *           Unlike isalpha, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is an alphabetic character; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isalpha.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isalpha(int c) {
  return (('A' <= c) && (c <= 'Z')) || (('a' <= c) && (c <= 'z'));
}

/*
 * Purpose : Test for a blank character using the "C" locale. In the "C"
 *           locale, only space (0x20) and tab (0x09) are blanks. Unlike
 *           isblank, the behavior of this function is defined when c is
 *           not EOF and not the value of an unsigned char.
 *
 * Returns : non-zero if c is a blank; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isblank.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isblank(int c) {
  return ('\t' == c) || (' ' == c);
}

/*
 * Purpose : Test for a decimal digit using the "C" locale. In the "C"
 *           locale, only the following are decimal digits.
 *
 *           0 1 2 3 4 5 6 7 8 9
 *
 *           Unlike isdigit, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is a decimal digit; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isdigit.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isdigit(int c) {
  return ('0' <= c) && (c <= '9');
}

/*
 * Purpose : Test for a lowercase letter using the "C" locale. In the "C"
 *           locale, only the following are lowercase letters.
 *
 *           a b c d e f g h i j k l m n o p q r s t u v w x y z
 *
 *           Unlike islower, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is a lowercase letter; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/islower.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_islower(int c) {
  return ('a' <= c) && (c <= 'z');
}

/*
 * Purpose : Test for a whitespace character using the "C" locale. In the "C"
 *           locale, only the following are whitespace characters.
 *
 *           tab ('\t')
 *           newline ('\n')
 *           vertical-tab ('\v')
 *           form-feed ('\f'),
 *           carriage-return ('\r')
 *           space (' ')
 *
 *           Unlike isspace, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is a whitespace character; otherwise, returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isspace.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isspace(int c) {
  return (0x20 == c) || ((0x09 <= c) && (c <= 0x0d));
}

/*
 * Purpose : Test for an uppercase letter using the "C" locale. In the "C"
 *           locale, only the following are lowercase letters.
 *
 *           A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
 *
 *           Unlike isupper, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is a uppercase letter; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isupper.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isupper(int c) {
  return ('A' <= c) && (c <= 'Z');
}

/*
 * Purpose : Test for a hexadecimal digit using the "C" locale. In the "C"
 *           locale, only the following are hexadecimal digits.
 *
 *           0 1 2 3 4 5 6 7 8 9 A B C D E F a b c d e f
 *
 *           Unlike isxdigit, the behavior of this function is defined for all
 * inputs.
 *
 * Returns : non-zero if c is a decimal digit; otherwise returns 0.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/isxdigit.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_isxdigit(int c) {
  return (('0' <= c) && (c <= '9')) || (('A' <= c) && (c <= 'F'))
         || (('a' <= c) && (c <= 'f'));
}

/*
 * Purpose : Transliterate uppercase characters to lowercase using the "C"
 *           locale. Unlike tolower, the behavior of this function is defined
 *           for all inputs.
 *
 * Returns : The lowercase letter corresponding to c, if defined; otherwise,
 *           the character is returned unchanged.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/tolower.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_tolower(int c) {
  if (('A' <= c) && (c <= 'Z')) {
    return c | 0x20;
  }
  return c;
}

/*
 * Purpose : Transliterate lowercase characters to uppercase using the "C"
 *           locale. Unlike tolower, the behavior of this function is defined
 *           for all inputs.
 *
 * Returns : The uppercase letter corresponding to c, if defined; otherwise,
 *           the character is returned unchanged.
 *
 * See : http://pubs.opengroup.org/onlinepubs/9699919799/functions/toupper.html
 *       http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html
 */
static inline int nr_toupper(int c) {
  if (('a' <= c) && (c <= 'z')) {
    return c & 0xdf;
  }
  return c;
}

/*
 * Purpose : Checks whether a string ends with the specified pattern. Note that
 * nr_striendswith is case-insensitive.
 *
 * Params  : 1. The input string to examine.
 *           2. The input string length.
 *           3. The pattern to look for at the end of the string.
 *           4. The pattern length.
 *
 * Returns : The true if input string ends with the pattern or false otherwise.
 */
static inline bool nr_striendswith(const char* s,
                                   const size_t slen,
                                   const char* pattern,
                                   const size_t pattern_len) {
  const char* suffix;

  if (slen < pattern_len) {
    /* input shorter than pattern */
    return false;
  }

  if (NULL == s) {
    /* invalid input */
    return false;
  }

  /* compare input's suffix with the pattern and return result */
  suffix = s + (slen - pattern_len);
  return 0 == nr_stricmp(suffix, pattern);
}

/*
 * Purpose : Strip the ".php" file extension from a file name
 *
 * Params  : 1. The string filename
 *           2. The filename length
 *
 * Returns : A newly allocated string stripped of the .php extension
 *
 */
static inline char* nr_file_basename(char* filename, int filename_len) {
  char* retval = NULL;

  if (NULL == filename || 0 >= filename_len) {
    return NULL;
  }

  if (4 >= filename_len) {
    /* if filename_len <= 4, there can't be a ".php" substring to remove. Assume
     * the filename does not contain ".php" and return the original filename. */
    return filename;
  }

  if (!nr_striendswith(filename, filename_len, NR_PSTR(".php"))) {
    return filename;
  }

  retval = nr_strndup(filename, filename_len - (sizeof(".php") - 1));
  nr_free(filename);
  return retval;
}

#endif /* UTIL_STRINGS_HDR */
