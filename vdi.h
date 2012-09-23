/* VDI file IO
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#include "vdifile.h"
#include <stdint.h>

#define	VDI_HEADER_MAGIC	0xbeda107f
#define	VDI_HEADER_LEN_MIN	0x180
#define	VDI_HEADER_LEN_SEEN	0x190
#define	VDI_HEADER_LEN_MAX	(VDI_HEADER_TOTAL-72)	/* correct?	*/
#define	VDI_HEADER_TOTAL	512
#define	VDI_HEADER_MAJOR	1
#define	VDI_HEADER_MINOR	1
static const char * const vdi_headers[] =
  {
    "<<< Sun xVM VirtualBox Disk Image >>>\n",
    "<<< Oracle VM VirtualBox Disk Image >>>\n",
    NULL /* order by length, shortest first!	*/
  };

typedef struct
  {
    unsigned long	time_lo;		/* 32 bit	*/
    unsigned		time_mid;		/* 16 bit	*/
    unsigned		time_hi_vers;		/* 16 bit	*/
    unsigned char	clock_hi_variant;	/* 8 bit	*/
    unsigned char	clock_lo;		/* 8 bit	*/
    unsigned char	node[6];		/* 48 bit	*/
  } UUID;

typedef struct vdi
  {
    int			init;
    struct vdi_file	*f;
    struct vdi_block	*b;

    /*  0*/ unsigned char	hdr[64];
    /* 64*/ unsigned long	sig;		/* VDI_HEADER_MAGIC		*/
    /* 68*/ int			maj, min;	/* VDH_HEADER_MAJOR/MINOR	*/
    /* len is calculated from here	*/
    /* 72*/ long		len;		/* VDI_HEADER_LEN_MIN to _MAX	*/
    /* 76*/ long		typ;		/* ?				*/
    /* 80*/ long		flags;		/* ?				*/
    /* 84*/ unsigned char	desc[256];	/* ?				*/
    /*340*/ unsigned long	block;		/* offset to block info		*/
    /*344*/ unsigned long	data;		/* offset to data		*/
    /*348*/ long		cyl;		/* number of cylinders		*/
    /*352*/ long		head;		/* number of heads to emulate	*/
    /*356*/ long		sec;		/* number of sectors per track	*/
    /*360*/ long		secsize;	/* number of bytes in a sector	*/
    /*364*/ long		unk1;
    /*368*/ int64_t		disksize;	/* Total size of the disk, in byte	*/
    /*376*/ long		blocksize;	/* Size of a block: 1M		*/
    /*380*/ long		extradata;	/* unknown			*/
    /*384*/ long		blocks;		/* Number of blocks in image	*/
    /*388*/ long		blockuse;	/* Total number of blocks used	*/

    /*392*/ UUID		uuid, uusnap, uulink, uupar;

    /*456*/ unsigned char	unk2[16];	/* no more in header len 0x180	*/
    /*480*/ unsigned char	unk3[40];	/* no more in header len 0x190	*/
    /*512*/
  } VDI;

unsigned char
get1(const unsigned char **ptr)
{
  const unsigned char	*p = *ptr;

  *ptr	+= 1;
  return *p;
}

unsigned
get2(const unsigned char **ptr)
{
  const unsigned char	*p = *ptr;

  *ptr	+= 2;
  return p[0]|(((unsigned)p[1])<<8);
}

unsigned long
get4(const unsigned char **ptr)
{
  unsigned 	u;

  u	= get2(ptr);
  return u|(((unsigned long)get2(ptr))<<16);
}

unsigned long
get4at(const unsigned char *ptr)
{
  return get4(&ptr);
}

uint64_t
get8(const unsigned char **ptr)
{
  unsigned long 	ul;

  ul	= get4(ptr);
  return ul|(((uint64_t)get4(ptr))<<32);
}

void
getn(void *to, const unsigned char **ptr, size_t len)
{
  memcpy(to, *ptr, len);
  *ptr	+= len;
}

/* WTF? */
void
getu(UUID *u, const unsigned char **ptr)
{
  memset(u, 0, sizeof u);
  u->time_lo		= get4(ptr);
  u->time_mid		= get2(ptr);
  u->time_hi_vers	= get2(ptr);
  u->clock_hi_variant	= get1(ptr);
  u->clock_lo		= get1(ptr);
  getn(u->node, ptr, sizeof u->node);
}

VDI *
vdi_init(VDI *v)
{
  memset(v, 0, sizeof *v);
  v->init	= 1;
  return v;
}

VDI *
vdi_close(VDI *v)
{
  if (v->b)
    v->b->lock = 0;
  v->b = 0;
  vdi_file_close(v->f);
  v->f = 0;
  return v;
}

static void
checkheader(VDI *v)
{
  char		id[sizeof v->hdr];
  const char * const *hdr;

  memset(id, 0, sizeof id);
  for (hdr=vdi_headers; *hdr; hdr++)
    {
      strcpy(id, *hdr);
      if (!memcmp(v->hdr, id, sizeof id))
	return;
    }
  warn("unknown file marker, it should read '%s'", id);
}

VDI *
vdi_open_f(VDI *v, struct vdi_file *f)
{
  const unsigned char	*p, *po;

  if (!v->init)
    vdi_init(v);
  vdi_close(v);
  v->f	= f;

  p	= vdi_file_get(f, (int64_t)0, VDI_HEADER_TOTAL);
  po	= p;

  getn(v->hdr, &p, sizeof v->hdr);
  checkheader(v);

  v->sig	= get4(&p);
  if (v->sig!=0xbeda107f)
    oops("VDI signature mismatch: 0x%08lx", v->sig);

  v->maj	= get2(&p);
  v->min	= get2(&p);
  v->len	= get4(&p);
  if (v->len<VDI_HEADER_LEN_MIN || v->len>VDI_HEADER_LEN_MAX)
    oops("VDI HEADER length mismatch, wanted 0x%x to 0x%x, is 0x%x", VDI_HEADER_LEN_MIN, VDI_HEADER_LEN_MAX, v->len);
  if (v->maj!=VDI_HEADER_MAJOR || v->min!=VDI_HEADER_MINOR)
    warn("unsupported VDI HEADER VERSION %d.%d, wanted %d.%d", v->maj, v->min, VDI_HEADER_MAJOR, VDI_HEADER_MINOR);

  v->typ	= get4(&p);
  v->flags	= get4(&p);
  getn(v->desc, &p, sizeof v->desc);

  v->block	= get4(&p);
  v->data	= get4(&p);

  v->cyl	= get4(&p);
  v->head	= get4(&p);
  v->sec	= get4(&p);
  v->secsize	= get4(&p);

  v->unk1	= get4(&p);
  v->disksize	= get8(&p);
  v->blocksize	= get4(&p);
  v->extradata	= get4(&p);

  v->blocks	= get4(&p);
  v->blockuse	= get4(&p);

  getu(&v->uuid, &p);
  getu(&v->uusnap, &p);
  getu(&v->uulink, &p);
  getu(&v->uupar, &p);

  getn(v->unk2, &p, sizeof v->unk2);
  getn(v->unk3, &p, sizeof v->unk3);
#if 0
  getn(v->unk4, &p, sizeof v->unk4);
#endif
  if (p-po!=VDI_HEADER_TOTAL)
    oops("BUG: header size %d, but processed %d", VDI_HEADER_TOTAL, (int)(p-po));

  v->b		= vdi_file_block(v->f, v->block, v->blocks*4l);
  v->b->lock	= 1;

  return v;
}

VDI *
vdi_open(VDI *v, const char *name)
{
  return vdi_open_f(v, vdi_file_open(name));
}

int
isnull(const void *p, size_t len)
{
  if (flag_verbose)
    return 0;
  if (!len)
    return 1;
  if (*(const char *)p)
    return 0;
  if (len==1)
    return 1;
  if (memcmp(p, ((const char *)p)+1, len-1))
    return 0;
  return 1;
}

void
printn(const char *t, const unsigned char *p, size_t len)
{
  if (isnull(p, len))
    return;
  printf("%s", t);
  while (len--)
    outc(*p++);
  outend();
  printf("\n");
}

void
prints(const char *t, const char *s)
{
  printn(t, (unsigned char *)s, strlen(s));
}

void
printx(unsigned const char *t, size_t len)
{
  while (len--)
    printf("%02x", *t++);
}

void
printu(const char *t, UUID *u)
{
  if (isnull(u, sizeof *u))
    return;
  printf("%s%08lx-%04x-%04x-%02x%02x-", t, u->time_lo, u->time_mid, u->time_hi_vers, u->clock_hi_variant, u->clock_lo);
  printx(u->node, sizeof u->node);
  printf("\n");
}

void
printb(const char *t, struct vdi_block *b)
{
  /* Hexdump?	*/
  printn(t, b->data, b->len);
}

const char *
gettype(VDI *v)
{
  switch (v->typ)
    {
      case 1:	return "dynamic";
      case 2:	return "fixed";
    }
  return "?";
}

void
vdi_info(VDI *v)
{
  unsigned long o;

  prints("file                  ", v->f->name);
  printn("description           ", v->desc, sizeof v->desc);
  printf("version, type, flag   %d.%d - %ld(%s) - %lx\n", v->maj, v->min, v->typ, gettype(v), v->flags);
  if (v->cyl || v->head || v->sec || v->secsize!=512)
  printf("cyl/hd/sec secsize    %ld/%ld/%ld %ld\n", v->cyl, v->head, v->sec, v->secsize);
  printf("total size            %llu\n", (llu)v->disksize);
  printf("blocks                %lu/%lu*0x%lx\n", v->blockuse, v->blocks, v->blocksize);
  printf("offset: block, data   0x%lx, 0x%lx\n", v->block, v->data);
  printu("UUID                  ", &v->uuid);
  printu("UUID snap             ", &v->uusnap);
  printu("UUID link             ", &v->uulink);
  printu("UUID parent           ", &v->uupar);

  if (v->unk1)
  printf("unknown      %lx\n", v->unk1);
  if (v->extradata)
  printf("extradata    %lx\n", v->extradata);
  printn("unknown2              ", v->unk2, sizeof v->unk2);
  printn("unknown3              ", v->unk3, sizeof v->unk3);
#if 0
  printn("unknown4              ", v->unk4, sizeof v->unk4);
#endif

  if (v->block>0x200)
    printb("gap: header to block  ", vdi_file_block(v->f, (int64_t)0x200, v->block-0x200));
  o = v->block+4*v->blocks;
  if (v->data > o)
    printb("gap: blocks to data   ", vdi_file_block(v->f, (int64_t)o, v->data-o));
}

/* Check the VDI-Blocks
 */
void
vdi_verify(VDI *v)
{
  int	i;
  char	*ok;
  int	count, warns, frag, fragmentation;
  
  ok = alloc0(v->blocks);
  count = 0;
  warns = 0;
  frag	= 0;
  fragmentation = 0;
  for (i=0; i<v->blocks; i++)
    {
      long	o;

      o = get4at(&v->b->data[i*4]);
      if (o == 0xffffffff)
	continue;
      if (o<0 || o>=v->blocks)
	{
          warn("illegal entry for block %d: %08lx (%s offset %llu)", i, o, v->b->f->name, (llu)(v->b->o+i*4));
          warns++;
	  continue;
	}
      if (o!=frag)
        fragmentation++;
      frag = o+1;
      count++;
      if (!++ok[o])
	ok[o]--;
    }
  if (count!=v->blockuse)
    {
      warn("count of used blocks mismatch, header=%d counted=%d", v->blockuse, count);
      warns++;
    }
  for (i=0; i<v->blockuse; i++)
    {
      if (ok[i]==1)
	continue;
      warn("block %d used %d times (should %d)", i, ok[i], 1);
      warns++;
    }
  for (; i<v->blocks; i++)
    {
      if (ok[i]==0)
	continue;
      warn("block %d used %d times (should %d)", i, ok[i], 0);
      warns++;
    }
  if (warns)
    oops("verification failed, %d problems", warns);
  if (!fragmentation)
    printf("block chain ok, not fragmented\n");
  else if (v->typ==1)
    printf("block chain ok, %d fragments (ok for dynamic)\n", fragmentation+1);
  else
    printf("block chain ok, but %d fragments (not ok as type is not dynamic)\n", fragmentation+1);
  free0(ok, v->blocks);
}

