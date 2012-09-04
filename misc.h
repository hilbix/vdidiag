/* Misc routines
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

static int out_n, out_q, out_s;
static char out_c;
int flag_verbose;

static void
outq(int set)
{
  if (out_q==set)
    return;
  putchar('"');
  out_q = set;
  out_s = 0;
}

static void
outs(void)
{
  outq(0);
  if (out_s)
    putchar(' ');
  out_s = 0;
}

static void
outflush(void)
{
  switch (out_n)
    {
      default:
	outs();
        printf("%d*", out_n);
      case 1:
	outs();
	printf("%02x", out_c);
        out_s = 1;
      case 0:
	break;
    }
  out_n = 0;
}

static void
outc(unsigned char c)
{
  if (isprint(c) && c!='"')
    {
      outflush();
      outq(1);
      putchar(c);
      return;
    }
  if (out_n && out_c!=c)
    outflush();
  out_c = c;
  out_n++;
}

static void
outend(void)
{
  outq(0);
  outflush();
  out_s = 0;
}

static void
vout(const char *prefix, int e, const char *s, va_list list)
{
  fflush(stdout);
  if (prefix)
    fprintf(stderr, "%s: ", prefix);
  vfprintf(stderr, s, list);
  if (e)
    fprintf(stderr, ": %s", strerror(e));
  fprintf(stderr, "\n");
}

static void
vdie(const char *prefix, int e, const char *s, va_list list)
{
  vout(prefix, e, s, list);
  exit(1);
}

static void
die(const char *prefix, const char *s, ...)
{
  va_list list;
  int e = errno;

  va_start(list, s);
  vdie(NULL, e, s, list);
  va_end(list);
}

static void
oops(const char *s, ...)
{
  va_list list;
  int e = errno;

  va_start(list, s);
  vdie("OOPS", e, s, list);
  va_end(list);
}

static void
warn(const char *s, ...)
{
  va_list list;

  va_start(list, s);
  vout("WARN", 0, s, list);
  va_end(list);
}

static void
bug(const char *s, ...)
{
  va_list list;

  va_start(list, s);
  vout("BUG", 0, s, list);
  va_end(list);
  exit(23);
}

void *
alloc0(size_t len)
{
  void *ptr;

  if (!len)
    len = 1;
  ptr = malloc(len);
  if (!ptr)
    oops("OOM");
  memset(ptr, 0, len);
  return ptr;
}

void
free0(const void *p, size_t len)
{
  memset((void *)p, 0, len);
  free((void *)p);
}

const char *
sdup(const char *s)
{
  s = strdup(s);
  if (!s)
    oops("OOM");
  return s;
}
