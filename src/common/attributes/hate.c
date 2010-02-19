/* vi: set ts=2:
 *
 * 
 * Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#include <config.h>
#include <kernel/eressea.h>
#include "hate.h"

#include <kernel/unit.h>
#include <kernel/version.h>

#include <util/attrib.h>
#include <util/resolve.h>
#include <util/storage.h>

static int
verify_hate(attrib * a)
{
	if (a->data.v==NULL) {
		return 0;
	}
	return 1;
}

static void
write_hate(const attrib * a, struct storage * store)
{
  write_unit_reference((unit*)a->data.v, store);
}

static int
read_hate(attrib * a, struct storage * store)
{
  int result = read_reference(&a->data.v, store, read_unit_reference, resolve_unit);
  if (result==0 && !a->data.v) {
    return AT_READ_FAIL;
  }
  return AT_READ_OK;
}

attrib_type at_hate = {
	"hates",
	NULL,
	NULL,
	verify_hate,
	write_hate,
	read_hate,
};

attrib *
make_hate(struct unit * u)
{
	attrib * a = a_new(&at_hate);
	a->data.v = u;
	return a;
}

void
init_hate(void)
{
	at_register(&at_hate);
}
