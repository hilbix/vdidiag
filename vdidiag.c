/* VDI file IO
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#include "misc.h"
#include "vdi.h"
#include <getopt.h>

VDI	v;

static int
run(const char *name)
{
  vdi_open(&v, name);
  vdi_info(&v);
  vdi_verify(&v);
  vdi_close(&v);
  return 0;
}

static void
usage(const char *arg0)
{
  die("Usage: %s [-v] IMAGE.vdi", arg0);
}

int
main(int argc, char **argv)
{
  int opt;

  while ((opt=getopt(argc, argv, "+v")) != -1)
    switch (opt)
      {
        default:
	  usage(argv[0]);
        case 'v':
	  flag_verbose	= 1;
	  break;
      }
  if (optind != argc-1)
    usage(argv[0]);
  return run(argv[optind]);
}
