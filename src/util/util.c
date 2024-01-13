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

#include <sigutils/util/util.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STRBUILD_BSIZ 16
#define DEBUG_BACKTRACE_NFUNCS 48

int saved_errno;

void
errno_save()
{
  saved_errno = errno;
}

void
errno_restore()
{
  errno = saved_errno;
}

/* Prototipos de funciones estaticas */
static void xalloc_die(void);

int
is_asciiz(const char *buf, int lbound, int ubound)
{
  register int i;

  for (i = lbound; i < ubound; i++)
    if (!buf[i])
      return i + 1;
  return 0;
}

char *
vstrbuild(const char *fmt, va_list ap)
{
  char *out = NULL, *tmp = NULL;
  char *result = NULL;

  int size, zeroindex;
  int last;
  va_list copy;

  last = 0;

  if (fmt != NULL) {
    if (!*fmt) {
      result = strdup("");
      goto done;
    }

    va_copy(copy, ap);
    size = vsnprintf(NULL, 0, fmt, copy) + 1;
    va_end(copy);

    if ((out = malloc(size)) == NULL)
      goto done;

    va_copy(copy, ap);
    vsnprintf(out, size, fmt, copy);
    va_end(copy);

    for (;;) {
      if ((zeroindex = is_asciiz(out, last, size)) != 0)
        break;

      last = size;
      size += STRBUILD_BSIZ;

      tmp = realloc(out, size); /* Reasignamos */
      if (tmp == NULL)
        goto done;

      out = tmp;

      va_copy(copy, ap);
      vsnprintf(out, size, fmt, copy);
      va_end(copy);
    }

    result = out;
    out = NULL;
  }

done:
  if (out != NULL)
    free(out);

  return result;
}

/* Construye una cadena mediante el formato printf y devuelve un
   puntero a la cadena resultado. DEBES liberar tu mismo la salida. */

/* FIXME: Buscar alguna alternativa mas portable */
char *
strbuild(const char *fmt, ...)
{
  char *out;
  va_list ap;

  va_start(ap, fmt);
  out = vstrbuild(fmt, ap);
  va_end(ap);

  return out;
}

/* Wrapper para malloc que autocomprueba el valor de retorno */
void *
xmalloc(size_t size)
{
  void *m;

  m = malloc(size);

  if (m == NULL)
    xalloc_die();

  return m;
}

/* Wrapper para realloc */
void *
xrealloc(void *ptr, size_t new_size)
{
  void *m;

  m = realloc(ptr, new_size);

  if (m == NULL)
    xalloc_die();

  return m;
}

/* Wrapper para strdup */
char *
xstrdup(const char *str)
{
  char *ret;

  if (str != NULL) {
    ret = xmalloc(strlen(str) + 1);
    strcpy(ret, str);
  } else
    ret = NULL;

  return ret;
}

/* Cuando nos quedamos sin memoria... */
static void
xalloc_die(void)
{
  abort();
}

/* Para manipular arrays de punteros */
int
ptr_list_append_check(void ***list, unsigned int *count, void *new)
{
  unsigned int i;
  void **reallocd_list;

  for (i = 0; i < *count; i++)
    if ((*list)[i] == NULL)
      break;

  if (i == *count) {
    if ((reallocd_list = xrealloc(*list, (1 + *count) * sizeof(void *)))
        == NULL)
      return -1;
    else {
      ++(*count);
      *list = reallocd_list;
    }
  }

  (*list)[i] = new;

  return i;
}

void
ptr_list_append(void ***list, unsigned int *count, void *new)
{
  (void)ptr_list_append_check(list, count, new);
}

int
ptr_list_remove_first(void ***list, unsigned int *count, void *ptr)
{
  unsigned int i;
  int found;

  found = 0;

  for (i = 0; i < *count; i++)
    if ((*list)[i] == ptr || ptr == NULL) {
      (*list)[i] = NULL;
      found++;

      break;
    }

  return found;
}

int
ptr_list_remove_all(void ***list, int *count, void *ptr)
{
  int i;
  int found;

  found = 0;

  for (i = 0; i < *count; i++)
    if ((*list)[i] == ptr || ptr == NULL) {
      (*list)[i] = NULL;
      found++;
    }

  return found;
}

char *
str_append_char(char *source, char c)
{
  int strsiz;
  char *nptr;

  strsiz = source == NULL ? 1 : strlen(source) + 1;

  nptr = (char *)xrealloc((void *)source, strsiz + 1);

  if (nptr == NULL)
    return NULL;

  nptr[strsiz - 1] = c;
  nptr[strsiz] = '\0';

  return nptr;
}

char *
fread_line(FILE *fp)
{
  char c;
  char *line;
  int buffer_size;
  int n;

  line = NULL;

  for (buffer_size = n = 0; (c = fgetc(fp)) != EOF; n++) {
    if (c == '\r') {
      n--;
      continue;
    }

    if (c == '\n') {
      if (line == NULL)
        line = xstrdup("");

      break;
    }

    if (buffer_size < (n + 1)) {
      if (buffer_size) {
        buffer_size <<= 1;
        line = xrealloc(line, buffer_size + 1);
      } else {
        buffer_size = STRBUILD_BSIZ;
        line = xmalloc(buffer_size + 1);
      }
    }

    line[n] = c;
  }

  if (line != NULL)
    line[n] = '\0';

  return line;
}

/* Todo: this is interesting. Export if necessary */

struct strlist *
strlist_new(void)
{
  struct strlist *new;

  new = xmalloc(sizeof(struct strlist));

  memset(new, 0, sizeof(struct strlist));

  return new;
}

void
strlist_append_string(struct strlist *list, const char *string)
{
  ptr_list_append(
      (void ***)&list->strings_list,
      &list->strings_count,
      xstrdup(string));
}

void
strlist_walk(
    struct strlist *list,
    void *data,
    void (*walk)(const char *, void *))
{
  unsigned int i;

  for (i = 0; i < list->strings_count; i++)
    if (list->strings_list[i] != NULL)
      (walk)(list->strings_list[i], data);
}

void
strlist_destroy(struct strlist *list)
{
  unsigned int i;

  for (i = 0; i < list->strings_count; i++)
    if (list->strings_list[i] != NULL)
      free(list->strings_list[i]);

  if (list->strings_list != NULL)
    free(list->strings_list);

  free(list);
}

int
strlist_have_element(const struct strlist *list, const char *string)
{
  unsigned int i;

  for (i = 0; i < list->strings_count; i++)
    if (list->strings_list[i] != NULL)
      if (strcmp(list->strings_list[i], string) == 0)
        return 1;

  return 0;
}

void
strlist_cat(struct strlist *dest, const struct strlist *list)
{
  unsigned int i;

  for (i = 0; i < list->strings_count; i++)
    if (list->strings_list[i] != NULL)
      strlist_append_string(dest, list->strings_list[i]);
}

void
strlist_union(struct strlist *dest, const struct strlist *list)
{
  unsigned int i;

  for (i = 0; i < list->strings_count; i++)
    if (list->strings_list[i] != NULL)
      if (!strlist_have_element(dest, list->strings_list[i]))
        strlist_append_string(dest, list->strings_list[i]);
}

void
strlist_debug(const struct strlist *list)
{
  unsigned int i;

  for (i = 0; i < list->strings_count; i++)
    if (list->strings_list[i] != NULL)
      fprintf(stderr, "%3u. %s\n", i, list->strings_list[i]);
    else
      fprintf(stderr, "<empty slot>\n");
}

/*
   Bit layout of returned byte:
   8   4   0
   MMMMDDDDD
*/

void
al_append_argument(arg_list_t *al, const char *arg)
{
  char *ptr;
  char **argl;

  ptr = (char *)xstrdup(arg);

  argl = (char **)xrealloc(
      (void *)al->al_argv,
      sizeof(char *) * (al->al_argc + 1));

  argl[al->al_argc++] = ptr;
  al->al_argv = argl;
}

void
free_al(arg_list_t *al)
{
  int i;

  for (i = 0; i < al->al_argc; i++)
    free(al->al_argv[i]);

  if (al->al_line != NULL)
    free(al->al_line);

  free(al->al_argv);
  free(al);
}

static arg_list_t *
__split_command(const char *line, char *separators, int fixed_sep_size)
{
  size_t i;

  int split_flag;
  int escape_flag;

  char *nptr;
  char *this_argument;
  arg_list_t *arg_info;

  arg_info = (arg_list_t *)xmalloc(sizeof(arg_list_t));

  arg_info->al_argc = 0;
  arg_info->al_argv = NULL;
  arg_info->al_line = NULL;

  this_argument = NULL;

  split_flag = 1;
  escape_flag = 0;

  i = 0;

  if (!fixed_sep_size)
    while (strchr(separators, line[i]) && line[i] != '\0')
      i++;

  for (; i < strlen(line); i++) {
    if (strchr(separators, line[i]) && split_flag && !escape_flag) {
      if (this_argument == NULL) {
        if (fixed_sep_size)
          al_append_argument(arg_info, "");
        continue;
      } else {
        al_append_argument(arg_info, this_argument);

        free(this_argument);
        this_argument = NULL;
      }
    } else if (line[i] == '"' && !escape_flag)
      split_flag = !split_flag;
    else if (line[i] == '\\' && !escape_flag)
      escape_flag = 1;
    /* else if (line[i] == '#' && split_flag && !escape_flag)
      break; */
    else {
      nptr = str_append_char(this_argument, line[i]);

      if (nptr == NULL) {
        free(this_argument);
        free_al(arg_info);
        return NULL;
      }

      this_argument = nptr;
      escape_flag = 0;
    }
  }

  if (this_argument != NULL) {
    al_append_argument(arg_info, this_argument);
    free(this_argument);
  }

  return arg_info;
}

arg_list_t *
csv_split_line(const char *line)
{
  return __split_command(line, ",", 1);
}

arg_list_t *
split_line(const char *line)
{
  return __split_command(line, " ", 0);
}

int
lscanf_huge(const char *fmt, ...)
{
  char *line;
  int result;
  va_list ap;

  va_start(ap, fmt);

  if ((line = fread_line(stdin)) == NULL)
    result = -1;
  else {
    result = vsscanf(line, fmt, ap);
    free(line);
  }

  va_end(ap);

  return result;
}

int
lscanf(const char *fmt, ...)
{
  char line[RECOMMENDED_LINE_SIZE];
  int result;
  va_list ap;

  va_start(ap, fmt);

  if (fgets(line, RECOMMENDED_LINE_SIZE - 1, stdin) == NULL)
    result = -1;
  else
    result = vsscanf(line, fmt, ap);

  va_end(ap);

  return result;
}

char *
ltrim(const char *str)
{
  while (*str)
    if (!isspace(*str))
      break;
    else
      str++;

  return xstrdup(str);
}

char *
rtrim(const char *str)
{
  char *copy;
  char *tail;

  copy = xstrdup(str);

  for (tail = copy + strlen(copy) - 1; (uintptr_t)copy <= (uintptr_t)tail;
       tail--) {
    if (!isspace(*tail))
      break;
    *tail = '\0';
  }

  return copy;
}

char *
trim(const char *str)
{
  char *copy;
  char *tail;

  while (*str)
    if (!isspace(*str))
      break;
    else
      str++;

  copy = xstrdup(str);

  for (tail = copy + strlen(copy) - 1; (uintptr_t)copy <= (uintptr_t)tail;
       tail--) {
    if (!isspace(*tail))
      break;
    *tail = '\0';
  }

  return copy;
}

/*
   Bit layout of returned byte:
   8   4   0
   MMMMDDDDD
*/

unsigned int
yday_to_daymonth(int yday, int year)
{
  int monthdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int month = 0;

  yday--;

  if ((year % 4 == 0) && ((!(year % 100 == 0)) || (year % 400 == 0)))
    monthdays[1] = 29;

  while (monthdays[month] <= yday) {
    yday -= monthdays[month++];
    if (month == 12)
      return 0;
  }

  return yday | (month << 5);
}

char *
get_curr_ctime(void)
{
  time_t now;
  char *text;
  time(&now);

  text = ctime(&now);

  text[24] = 0;

  return text;
}

void
grow_buf_init_loan(grow_buf_t *buf, const void *data, size_t size, size_t alloc)
{
  buf->buffer = (void *)data;
  buf->alloc = alloc;
  buf->size = size;
  buf->ptr = 0;
  buf->loan = 1;
}

void
grow_buf_init(grow_buf_t *buf)
{
  memset(buf, 0, sizeof(grow_buf_t));
}

int
grow_buf_ensure_min_alloc(grow_buf_t *buf, size_t min_alloc)
{
  void *tmp;

  if (buf->alloc < min_alloc) {
    tmp = realloc(buf->buffer, min_alloc);
    if (tmp == NULL)
      return -1;

    buf->buffer = tmp;
    buf->alloc = min_alloc;
  }

  return 0;
}

void *
grow_buf_alloc(grow_buf_t *buf, size_t size)
{
  size_t alloc = buf->alloc;
  size_t total_size = buf->size + size;
  void *tmp;

  if (alloc == 0)
    alloc = 1;

  while (alloc < total_size)
    alloc <<= 1;

  if (alloc != buf->alloc) {
    if ((tmp = realloc(buf->buffer, alloc)) == NULL)
      return NULL;

    buf->buffer = tmp;
    buf->alloc = alloc;
  }

  tmp = (char *)buf->buffer + buf->size;
  buf->size = total_size;

  return tmp;
}

void *
grow_buf_append_hollow(grow_buf_t *buf, size_t size)
{
  void *reserved = NULL;
  size_t avail = grow_buf_avail(buf);

  if (size > avail)
    if (grow_buf_alloc(buf, size - avail) == NULL)
      return NULL;

  if ((reserved = grow_buf_current_data(buf)) == NULL)
    return NULL;

  grow_buf_seek(buf, size, SEEK_CUR);

  return reserved;
}

int
grow_buf_append(grow_buf_t *buf, const void *data, size_t size)
{
  void *reserved = grow_buf_append_hollow(buf, size);

  if (reserved == NULL)
    return -1;

  memcpy(reserved, data, size);

  return 0;
}

int
grow_buf_append_printf(grow_buf_t *buf, const char *fmt, ...)
{
  va_list ap;
  char *result = NULL;
  int code = -1;

  va_start(ap, fmt);

  if ((result = vstrbuild(fmt, ap)) == NULL)
    goto done;

  if (grow_buf_append(buf, result, strlen(result)) == -1)
    goto done;

  va_end(ap);

  code = 0;

done:
  if (result != NULL)
    free(result);

  return code;
}

ssize_t
grow_buf_read(grow_buf_t *buf, void *data, size_t size)
{
  size_t avail = grow_buf_avail(buf);

  if (size > avail)
    size = avail;

  if (size > 0) {
    memcpy(data, grow_buf_current_data(buf), size);
    grow_buf_seek(buf, size, SEEK_CUR);
  }

  return size;
}

int
grow_buf_append_null(grow_buf_t *buf)
{
  return grow_buf_append(buf, "", 1);
}

void *
grow_buf_get_buffer(const grow_buf_t *buf)
{
  return buf->buffer;
}

void *
grow_buf_current_data(const grow_buf_t *buf)
{
  if (buf->ptr >= buf->size)
    return NULL;

  return buf->bytes + buf->ptr;
}

size_t
grow_buf_get_size(const grow_buf_t *buf)
{
  return buf->size;
}

size_t
grow_buf_ptr(const grow_buf_t *buf)
{
  return buf->ptr;
}

size_t
grow_buf_avail(const grow_buf_t *buf)
{
  if (buf->ptr > buf->size)
    return 0;

  return buf->size - buf->ptr;
}

void
grow_buf_finalize(grow_buf_t *buf)
{
  if (!buf->loan && buf->buffer != NULL)
    free(buf->buffer);
}

void
grow_buf_shrink(grow_buf_t *buf)
{
  buf->size = 0;
  buf->ptr = 0;
}

void
grow_buf_clear(grow_buf_t *buf)
{
  grow_buf_finalize(buf);
  memset(buf, 0, sizeof(grow_buf_t));
}

size_t
grow_buf_seek(grow_buf_t *buf, off_t offset, int whence)
{
  off_t new_off;

  switch (whence) {
    case SEEK_SET:
      new_off = offset;
      break;

    case SEEK_CUR:
      new_off = buf->ptr + offset;
      break;

    case SEEK_END:
      new_off = buf->size + offset;
      break;

    default:
      errno = EINVAL;
      return -1;
  }

  if (new_off < 0 || (size_t)new_off > buf->size) {
    errno = EINVAL;
    return -1;
  }

  buf->ptr = new_off;

  return buf->ptr;
}

int
grow_buf_transfer(grow_buf_t *dest, grow_buf_t *src)
{
  memcpy(dest, src, sizeof(grow_buf_t));
  memset(src, 0, sizeof(grow_buf_t));

  return 0;
}
