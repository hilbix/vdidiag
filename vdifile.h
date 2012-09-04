/* Check VDI (Virtualbox Disk Image) file structure.
 *
 * This is terribly incomplete today.
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#ifndef	VDI_MAX_CACHE
#define	VDI_MAX_CACHE	0x10000000ull	/* 256 MB	*/
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef unsigned long long	llu;

struct vdi_file
  {
    int		fd;
    const char	*name;
    int		ro;
    int64_t	max;
    struct vdi_block	*lru;
  };

struct vdi_block
  {
    struct vdi_block	*next;
    struct vdi_file	*f;
    int			lock;
    int64_t		o;
    int			len;
    unsigned char	*data;
  };

struct vdi_file *
vdi_file_open(const char *name)
{
  struct vdi_file	*f;
  int			fd, ro;

  ro = 0;
  while ((fd=open(name, O_RDWR))<0 && (errno!=EACCES || (fd=open(name, O_RDONLY))<0))
    {
      if (errno==EACCES && (fd=open(name, O_RDONLY))>=0)
	{
	  warn("opening readonly: %s", name);
	  ro	= 1;
	  break;
	}
      if (errno!=EINTR)
        oops("open %s", name);
    }
  f		= alloc0(sizeof *f);
  f->fd		= fd;
  f->name	= sdup(name);
  f->ro		= ro;
  f->max	= VDI_MAX_CACHE;
  return f;
}

void
vdi_block_free(struct vdi_block *b)
{
  struct vdi_block **p, *tmp;

  if (b->lock)
    bug("free of locked block: %p %llu:%d", b, (llu)(b->o), b->len);
  for (p= &b->f->lru; (tmp= *p)!=0; p= &tmp->next)
    if (tmp==b)
      {
	*p	= tmp->next;
	free0(b->data, b->len);
	free0(b, sizeof *b);
	return;
      }
  bug("free of unknwon block: %p %llu:%d", b, (llu)(b->o), b->len);
}

void
vdi_file_close(struct vdi_file *f)
{
  if (!f)
    return;
  if (close(f->fd))
    oops("close %s", f->name);
  while (f->lru)
    vdi_block_free(f->lru);
  free0(f->name, strlen(f->name));
  free0(f, sizeof *f);
}

/* internal function	*/
static void
vdi_file_read(struct vdi_file *f, int64_t o, unsigned char *ptr, int len)
{
  int		fd = f->fd;
  const char	*name = f->name;

  while (len>0)
    {
      int	got;
      
      if (lseek(fd, o, SEEK_SET)!=o)
	oops("cannot seek %s: %llu", name, (llu)o);
      errno	= 0;
      if ((got=read(fd, ptr, len))<0)
	{
	  if (errno==EINTR)
	    continue;
	  oops("read error %s", name);
	}
      if (!got)
	oops("unexpected EOF %s: %llu:%d", name, (llu)o, len);
      
      len	-= got;
      ptr	+= got;
      o		+= got;
    }
}

/* You must not send overlapping read requests.
 *
 * This reads a block and returns a block pointer.
 * Multiple reads return a pointer to an LRU list.
 */
struct vdi_block *
vdi_file_block(struct vdi_file *f, int64_t o, int len)
{
  struct vdi_block	**p, *tmp, *tlo;
  long long		max = 0;

  tlo	= 0;
  for (p= &f->lru; (tmp= *p)!=0; p= &tmp->next)
    {
      if (tmp->o==o && tmp->len>=len)
	{
	  *p		= tmp->next;
	  tmp->next	= f->lru;
	  f->lru	= tmp;
	  return tmp;
	}
      if (o+len>tmp->o && o<tmp->o+tmp->len)
	bug("overlapped reads %s: %llu:%d vs. %llu:%d", f->name, (llu)o, len, (llu)tmp->o, tmp->len);
      if (!tmp->lock)
	tlo	= tmp;
      max	+= tmp->len;
    }
  /* not found	*/
  if (tlo && max>=f->max)
    vdi_block_free(tlo);
  tmp		= alloc0(sizeof *tmp);
  tmp->o	= o;
  tmp->len	= len;
  tmp->data	= alloc0((size_t)len);

  vdi_file_read(f, o, tmp->data, len);
  
  tmp->next	= f->lru;
  tmp->f	= f;
  f->lru	= tmp;
  return tmp;
}

void *
vdi_file_set(struct vdi_file *f, int64_t o, int len)
{
  return vdi_file_block(f, o, len)->data;
}

const void *
vdi_file_get(struct vdi_file *f, int64_t o, int len)
{
  return vdi_file_set(f, o, len);
}

