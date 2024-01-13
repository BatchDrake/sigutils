/*
  Copyright (C) 2013 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>
*/

#ifndef _UTIL_H
#define _UTIL_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define RECOMMENDED_LINE_SIZE 256

#ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef SIGN
#  define SIGN(x) (!(x < 0) - !(x > 0))
#endif

#define _JOIN(a, b) a##b
#define JOIN(a, b) _JOIN(a, b)

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define IN_BOUNDS(x, range) (((x) >= 0) && ((x) < (range)))

#define PTR_LIST(type, name) \
  type **name##_list;        \
  unsigned int name##_count;

#define PTR_LIST_PRIVATE(type, name) \
  SUPRIVATE type **name##_list;      \
  SUPRIVATE unsigned int name##_count;

#define PTR_LIST_CONST(type, name) \
  const type **name##_list;        \
  unsigned int name##_count;

#define PTR_LIST_PRIVATE_CONST(type, name) \
  SUPRIVATE const type **name##_list;      \
  SUPRIVATE unsigned int name##_count;

#define PTR_LIST_LOCAL(type, name) \
  type **name##_list = NULL;       \
  unsigned int name##_count = 0;

#define PTR_LIST_EXTERN(type, name) \
  extern type **name##_list;        \
  extern unsigned int name##_count;

#define PTR_LIST_INIT(where, name) \
  where->name##_list = NULL;       \
  where->name##_count = 0;

#define PTR_LIST_APPEND(name, ptr) \
  ptr_list_append((void ***)&JOIN(name, _list), &JOIN(name, _count), ptr)

#define PTR_LIST_APPEND_CHECK(name, ptr) \
  ptr_list_append_check((void ***)&JOIN(name, _list), &JOIN(name, _count), ptr)

#define PTR_LIST_REMOVE(name, ptr) \
  ptr_list_remove_first((void ***)&JOIN(name, _list), &JOIN(name, _count), ptr)

#define FOR_EACH_PTR_STANDALONE(this, name)                             \
  unsigned int JOIN(_idx_, __LINE__);                                   \
  for (JOIN(_idx_, __LINE__) = 0; JOIN(_idx_, __LINE__) < name##_count; \
       JOIN(_idx_, __LINE__)++)                                         \
    if ((this = name##_list[JOIN(_idx_, __LINE__)]) != NULL)

#define FOR_EACH_PTR(this, where, name)                                        \
  unsigned int JOIN(_idx_, __LINE__);                                          \
  for (JOIN(_idx_, __LINE__) = 0; JOIN(_idx_, __LINE__) < where->name##_count; \
       JOIN(_idx_, __LINE__)++)                                                \
    if ((this = where->name##_list[JOIN(_idx_, __LINE__)]) != NULL)

#define __UNITS(x, wrdsiz) ((((x) + (wrdsiz - 1)) / wrdsiz))
#define __ALIGN(x, wrdsiz) (__UNITS(x, wrdsiz) * wrdsiz)

#ifdef __GNUC__
#  define IGNORE_RESULT(type, expr)               \
    do {                                          \
      type ignored_val__ __attribute__((unused)); \
      ignored_val__ = expr;                       \
    } while (0)
#else
#  define IGNORE_RESULT(type, expr) (void)expr
#endif /* __GNUC__ */

struct strlist {
  PTR_LIST(char, strings);
};

typedef struct _al {
  int al_argc;
  char **al_argv;

  char *al_line;
} arg_list_t;

struct grow_buf {
  size_t ptr;
  size_t size;
  size_t alloc;
  int loan;
  union {
    void *buffer;
    unsigned char *bytes;
  };
};

typedef struct grow_buf grow_buf_t;

#define grow_buf_INITIALIZER \
  {                          \
    0, 0, 0, 0,              \
    {                        \
      NULL                   \
    }                        \
  }
#define GROW_BUF_STRCAT(gbuf, str) grow_buf_append((gbuf), (str), strlen(str))

void al_append_argument(arg_list_t *, const char *);
void free_al(arg_list_t *);

arg_list_t *csv_split_line(const char *);
arg_list_t *split_line(const char *);

void grow_buf_init(grow_buf_t *buf);
void grow_buf_init_loan(
    grow_buf_t *buf,
    const void *data,
    size_t size,
    size_t alloc);

int grow_buf_ensure_min_alloc(grow_buf_t *buf, size_t min_alloc);
void *grow_buf_alloc(grow_buf_t *buf, size_t size);
void *grow_buf_append_hollow(grow_buf_t *buf, size_t size);
int grow_buf_append(grow_buf_t *buf, const void *data, size_t size);
ssize_t grow_buf_read(grow_buf_t *buf, void *data, size_t);
int grow_buf_append_printf(grow_buf_t *buf, const char *fmt, ...);
int grow_buf_append_null(grow_buf_t *buf);
void *grow_buf_get_buffer(const grow_buf_t *buf);
void *grow_buf_current_data(const grow_buf_t *buf);
size_t grow_buf_get_size(const grow_buf_t *buf);
size_t grow_buf_ptr(const grow_buf_t *buf);
size_t grow_buf_avail(const grow_buf_t *buf);
void grow_buf_finalize(grow_buf_t *buf);
void grow_buf_shrink(grow_buf_t *buf);
void grow_buf_clear(grow_buf_t *buf);
size_t grow_buf_seek(grow_buf_t *buf, off_t offset, int whence);
int grow_buf_transfer(grow_buf_t *dest, grow_buf_t *src);

void *xmalloc(size_t siz);
void *xrealloc(void *p, size_t siz);
char *xstrdup(const char *s);
int is_asciiz(const char *buf, int lbound, int ubound);
char *vstrbuild(const char *fmt, va_list ap);
char *strbuild(const char *fmt, ...);
char *str_append_char(char *source, char c);
char *fread_line(FILE *fp);
void ptr_list_append(void ***, unsigned int *, void *);
int ptr_list_append_check(void ***, unsigned int *, void *);
int ptr_list_remove_first(void ***, unsigned int *, void *);
int ptr_list_remove_all(void ***, int *, void *);

void errno_save(void);
void errno_restore(void);

struct strlist *strlist_new(void);
void strlist_append_string(struct strlist *, const char *);
void strlist_walk(struct strlist *, void *, void (*)(const char *, void *));
void strlist_destroy(struct strlist *);
void strlist_debug(const struct strlist *);
void strlist_cat(struct strlist *, const struct strlist *);
void strlist_union(struct strlist *, const struct strlist *);
int strlist_have_element(const struct strlist *, const char *);
unsigned int yday_to_daymonth(int, int);

char *trim(const char *);
char *rtrim(const char *);
char *ltrim(const char *);
int lscanf(const char *, ...);
int lscanf_huge(const char *, ...);

#ifdef __sun__ /* puto Solaris */
int dprintf(int fd, const char *fmt, ...);
#endif

#endif /* _UTIL_H */
