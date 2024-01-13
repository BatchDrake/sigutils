/*

  Copyright (C) 2021 Gonzalo José Carracedo Carballal

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

#ifndef _SIGUTILS_DEFS_H
#define _SIGUTILS_DEFS_H

#include <sigutils/util/util.h>

#ifndef su_calloc
#  define su_calloc(len, size) calloc(len, size)
#endif /* su_calloc */

#ifndef su_free
#  define su_free(ptr) free(ptr)
#endif /* su_free */

#ifndef su_malloc
#  define su_malloc(size) malloc(size)
#endif /* su_malloc */

#define SU_TYPENAME(class) JOIN(class, _t)
#define SU_METHOD_NAME(class, name) JOIN(class, JOIN(_, name))
#define SU_METHOD(class, ret, name, ...) \
  ret SU_METHOD_NAME(class, name)(SU_TYPENAME(class) * self, ##__VA_ARGS__)

#define SU_METHOD_CONST(class, ret, name, ...) \
  ret SU_METHOD_NAME(class, name)(             \
      const SU_TYPENAME(class) * self,         \
      ##__VA_ARGS__)

#define SU_GETTER SU_METHOD_CONST

#define SU_CONSTRUCTOR_TYPED(ret, class, ...) \
  ret SU_METHOD_NAME(class, init)(SU_TYPENAME(class) * self, ##__VA_ARGS__)

#define SU_CONSTRUCTOR(class, ...) \
  SU_CONSTRUCTOR_TYPED(SUBOOL, class, ##__VA_ARGS__)

#define SU_DESTRUCTOR(class) \
  void SU_METHOD_NAME(class, finalize)(SU_TYPENAME(class) * self)

#define SU_INSTANCER(class, ...) \
  SU_TYPENAME(class) * SU_METHOD_NAME(class, new)(__VA_ARGS__)

#define SU_COPY_INSTANCER(class, ...) \
  SU_METHOD_CONST(class, SU_TYPENAME(class) *, dup, ##__VA_ARGS__)

#define SU_COLLECTOR(class) \
  void SU_METHOD_NAME(class, destroy)(SU_TYPENAME(class) * self)

#define SU_ALLOCATE_MANY_CATCH(dest, len, type, action)   \
  if ((dest = su_calloc(len, sizeof(type))) == NULL) {    \
    SU_ERROR(                                             \
        "failed to allocate %d objects of type \"%s\"\n", \
        len,                                              \
        STRINGIFY(type));                                 \
    action;                                               \
  }

#define SU_ALLOCATE_CATCH(dest, type, action)             \
  if ((dest = su_calloc(1, sizeof(type))) == NULL) {      \
    SU_ERROR(                                             \
        "failed to allocate one object of type \"%s\"\n", \
        STRINGIFY(type));                                 \
    action;                                               \
  }

#define SU_MAKE_CATCH(dest, class, action, ...)                                \
  if ((dest = JOIN(class, _new)(__VA_ARGS__)) == NULL) {                       \
    SU_ERROR("failed to create instance of class \"%s\"\n", STRINGIFY(class)); \
    action;                                                                    \
  }

#define SU_CONSTRUCT_CATCH(class, dest, action, arg...) \
  if (!JOIN(class, _init)(dest, ##arg)) {               \
    SU_ERROR(                                           \
        "failed to call constructor of class \"%s\"\n", \
        STRINGIFY(class));                              \
    action;                                             \
  }

#define SU_DESTRUCT(class, dest) JOIN(class, _finalize)(dest)
#define SU_DISPOSE(class, dest) JOIN(class, _destroy)(dest)

/* __REL_FILE__ is provided byt the build system, use it.
  Otherwise, set the variable to an empty string as we
  don't want to leak full paths into the binary.
*/
#ifndef __REL_FILE__
#  define __REL_FILE__ ""
#endif /* __REL_FILE__ */

#define SU_TRYCATCH(expr, action)        \
  if (!(expr)) {                         \
    SU_ERROR(                            \
        "exception in \"%s\" (%s:%d)\n", \
        STRINGIFY(expr),                 \
        __REL_FILE__,                    \
        __LINE__);                       \
    action;                              \
  }

/* Macros for "goto done" style error recovery */
#define SU_TRY(expr) SU_TRYCATCH(expr, goto done)
#define SU_TRYC(expr) SU_TRY((expr) != -1)
#define SU_TRYZ(expr) SU_TRY((expr) == 0)

#define SU_ALLOCATE_MANY(dest, len, type) \
  SU_ALLOCATE_MANY_CATCH(dest, len, type, goto done)

#define SU_ALLOCATE(dest, type) SU_ALLOCATE_CATCH(dest, type, goto done)

#define SU_MAKE(dest, class, ...) \
  SU_MAKE_CATCH(dest, class, goto done, __VA_ARGS__)

#define SU_CONSTRUCT(class, dest, arg...) \
  SU_CONSTRUCT_CATCH(class, dest, goto done, ##arg)

/* Macros for "goto fail" style error recovery */
#define SU_TRY_FAIL(expr) SU_TRYCATCH(expr, goto fail)
#define SU_TRYC_FAIL(expr) SU_TRY_FAIL((expr) != -1)
#define SU_TRYZ_FAIL(expr) SU_TRY_FAIL((expr) == 0)

#define SU_ALLOCATE_MANY_FAIL(dest, len, type) \
  SU_ALLOCATE_MANY_CATCH(dest, len, type, goto fail)

#define SU_ALLOCATE_FAIL(dest, type) SU_ALLOCATE_CATCH(dest, type, goto fail)

#define SU_MAKE_FAIL(dest, class, ...) \
  SU_MAKE_CATCH(dest, class, goto fail, __VA_ARGS__)

#define SU_CONSTRUCT_FAIL(class, dest, arg...) \
  SU_CONSTRUCT_CATCH(class, dest, goto fail, ##arg)

#endif /* _SIGUTILS_DEFS_H */
