/*
  Copyright (C) 2022 Ángel Ruiz Fernández

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

#ifndef _UTIL_STATVFS_H
#define _UTIL_STATVFS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

struct statvfs {
  unsigned long f_bsize;   /*   Filesystem block size */
  unsigned long f_frsize;  /*   Fragment size */
  fsblkcnt_t f_blocks;     /* * Size of fs in f_frsize units */
  fsblkcnt_t f_bfree;      /*   Number of free blocks */
  fsblkcnt_t f_bavail;     /* * Number of free blocks for
                                                         unprivileged users */
  fsfilcnt_t f_files;      /*   Number of inodes */
  fsfilcnt_t f_ffree;      /*   Number of free inodes */
  fsfilcnt_t f_favail;     /*   Number of free inodes for
                                                         unprivileged users */
  unsigned long f_fsid;    /*   Filesystem ID */
  unsigned long f_flag;    /*   Mount flags */
  unsigned long f_namemax; /*   Maximum filename length */
};

int statvfs(const char *__restrict path, struct statvfs *__restrict buf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _UTIL_STATVFS_H */
