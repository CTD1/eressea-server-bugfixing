/* vi: set ts=2:
 *
 *	
 *	Eressea PB(E)M host Copyright (C) 1998-2003
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
#include "createunit.h"

/* kernel includes */
#include <kernel/faction.h>
#include <kernel/unit.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/version.h>

/* util includes */
#include <util/attrib.h>
#include <util/base36.h>
#include <util/event.h>
#include <util/log.h>
#include <util/resolve.h>
#include <util/storage.h>

/* ansi includes */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#include <stdio.h>
/***
 ** restore a mage that was turned into a toad
 **/

typedef struct createunit_data {
	struct region * r;
	struct faction * f;
	const struct race * race;
	int number;
} createunit_data;

static void
createunit_init(trigger * t)
{
	t->data.v = calloc(sizeof(createunit_data), 1);
}

static void
createunit_free(trigger * t)
{
	free(t->data.v);
}

static int
createunit_handle(trigger * t, void * data)
{
	/* call an event handler on createunit.
	 * data.v -> ( variant event, int timer )
	 */
	createunit_data * td = (createunit_data*)t->data.v;
	if (td->r!=NULL && td->f!=NULL) {
		create_unit(td->r, td->f, td->number, td->race, 0, NULL, NULL);
	} else {
		log_error(("could not perform createunit::handle()\n"));
	}
	unused(data);
	return 0;
}

static void
createunit_write(const trigger * t, struct storage * store)
{
	createunit_data * td = (createunit_data*)t->data.v;
	write_faction_reference(td->f, store);
	write_region_reference(td->r, store);
	write_race_reference(td->race, store);
	store->w_int(store, td->number);
}

static int
createunit_read(trigger * t, struct storage * store)
{
  createunit_data * td = (createunit_data*)t->data.v;

  int uc = read_reference(&td->f, store, read_faction_reference, resolve_faction);
  int rc = read_reference(&td->r, store, read_region_reference, RESOLVE_REGION(store->version));
  td->race = (const struct race*)read_race_reference(store).v;

  if (uc==0 && rc==0) {
    if (!td->f || !td->r) return AT_READ_FAIL;
  }
  td->number = store->r_int(store);

  return AT_READ_OK;
}

trigger_type tt_createunit = {
	"createunit",
	createunit_init,
	createunit_free,
	createunit_handle,
	createunit_write,
	createunit_read
};

trigger *
trigger_createunit(region * r, struct faction * f, const struct race * rc, int number)
{
	trigger * t = t_new(&tt_createunit);
	createunit_data * td = (createunit_data*)t->data.v;
	td->r = r;
	td->f = f;
	td->race = rc;
	td->number = number;
	return t;
}
