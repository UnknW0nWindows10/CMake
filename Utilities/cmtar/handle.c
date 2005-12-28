/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  handle.c - libtar code for initializing a TAR handle
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include <libtarint/internal.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#ifdef _MSC_VER
#include <io.h>
//Yogi: hack. this should work on windows where there is no O_ACCMODE defined
#ifndef O_ACCMODE
# define O_ACCMODE 0x0003
#endif
#endif

const char libtar_version[] = PACKAGE_VERSION;

#define libtar_symbol(name, ret, args, callargs) \
  static ret libtar_##name args \
  { \
    return name callargs; \
  }

#if defined(__BORLANDC__)
libtar_symbol(open, int, (const char* pathname, int flags, mode_t mode), (pathname, flags, mode));
libtar_symbol(close, int, (int fd), (fd));
libtar_symbol(read, ssize_t, (int fd, void* buf, size_t count), (fd, buf, count));
libtar_symbol(write, ssize_t, (int fd, void* buf, size_t count), (fd, buf, count));

static tartype_t default_type = { libtar_open, libtar_close, libtar_read, libtar_write };
#else
static tartype_t default_type = { open, close, read, write };
#endif


static int
tar_init(TAR **t, char *pathname, tartype_t *type,
   int oflags, int mode, int options)
{
  (void)mode;
  if ((oflags & O_ACCMODE) == O_RDWR)
  {
    errno = EINVAL;
    return -1;
  }

  *t = (TAR *)calloc(1, sizeof(TAR));
  if (*t == NULL)
    return -1;

  (*t)->pathname = pathname;
  (*t)->options = options;
  (*t)->type = (type ? type : &default_type);
  (*t)->oflags = oflags;

  if ((oflags & O_ACCMODE) == O_RDONLY)
    (*t)->h = libtar_hash_new(256,
            (libtar_hashfunc_t)path_hashfunc);
  else
    (*t)->h = libtar_hash_new(16, (libtar_hashfunc_t)dev_hash);
  if ((*t)->h == NULL)
  {
    free(*t);
    return -1;
  }

  return 0;
}


/* open a new tarfile handle */
int
tar_open(TAR **t, char *pathname, tartype_t *type,
   int oflags, int mode, int options)
{
  if (tar_init(t, pathname, type, oflags, mode, options) == -1)
    return -1;

  if ((options & TAR_NOOVERWRITE) && (oflags & O_CREAT))
    oflags |= O_EXCL;

#ifdef O_BINARY
  oflags |= O_BINARY;
#endif

  (*t)->fd = (*((*t)->type->openfunc))(pathname, oflags, mode);
  if ((*t)->fd == -1)
  {
    free(*t);
    return -1;
  }

  return 0;
}


int
tar_fdopen(TAR **t, int fd, char *pathname, tartype_t *type,
     int oflags, int mode, int options)
{
  if (tar_init(t, pathname, type, oflags, mode, options) == -1)
    return -1;

  (*t)->fd = fd;
  return 0;
}


int
tar_fd(TAR *t)
{
  return t->fd;
}


/* close tarfile handle */
int
tar_close(TAR *t)
{
  int i;

  i = (*(t->type->closefunc))(t->fd);

  if (t->h != NULL)
    libtar_hash_free(t->h, ((t->oflags & O_ACCMODE) == O_RDONLY
          ? free
          : (libtar_freefunc_t)tar_dev_free));
  free(t);

  return i;
}


