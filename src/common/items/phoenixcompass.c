/* vi: set ts=2:
*
* Eressea PB(E)M host Copyright (C) 1998-2003
*      Christian Schlittchen (corwin@amber.kn-bremen.de)
*      Katja Zedel (katze@felidae.kn-bremen.de)
*      Henning Peters (faroul@beyond.kn-bremen.de)
*      Enno Rehling (enno@eressea-pbem.de)
*      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
*
* This program may not be used, modified or distributed without
* prior permission by the authors of Eressea.
*/

#include <config.h>
#include <eressea.h>
#include "phoenixcompass.h"

/* kernel includes */
#include <kernel/item.h>
#include <kernel/region.h>
#include <kernel/faction.h>
#include <kernel/unit.h>
#include <kernel/message.h>

/* util includes */
#include <util/functions.h>
#include <util/rand.h>

/* libc includes */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

static int
use_phoenixcompass(struct unit * u, const struct item_type * itype,
                   int amount, struct order * ord)
{
  region      *r;
  unit        *closest_phoenix = NULL;
  int          closest_phoenix_distance = INT_MAX;
  boolean      confusion = false;
  direction_t  direction;
  unit        *u2;
  direction_t  closest_neighbour_direction = 0;
  static race * rc_phoenix = NULL;
  
  if (rc_phoenix==NULL) {
    rc_phoenix = rc_find("phoenix");
    if (rc_phoenix==NULL) return 0;
  }

  /* find the closest phoenix. */

  for(r=regions; r; r=r->next) {
    for(u2=r->units; u2; u2=u2->next) {
      if (u2->race == rc_phoenix) {
        if(closest_phoenix == NULL) {
          closest_phoenix = u2;
          closest_phoenix_distance = distance(u->region, closest_phoenix->region);
        } else {
          int dist = distance(u->region, r);
          if(dist < closest_phoenix_distance) {
            closest_phoenix = u2;
            closest_phoenix_distance = dist;
            confusion = false;
          } else if(dist == closest_phoenix_distance) {
            confusion = true;
          }
        }
      }
    }
  }

  /* no phoenix found at all.* if confusion == true more than one phoenix
   * at the same distance was found and the device is confused */

  if(closest_phoenix == NULL
      || closest_phoenix->region == u->region
      || confusion == true) {
    add_message(&u->faction->msgs, msg_message("phoenixcompass_confusion",
        "unit region command", u, u->region, ord));
    return 0;
  }

  /* else calculate the direction. this is tricky. we calculate the
   * neighbouring region which is closest to the phoenix found. hardcoded
   * for readability. */

  for(direction = 0; direction < MAXDIRECTIONS; ++direction) {
    region     *neighbour;
    int         closest_neighbour_distance = INT_MAX;
    
    neighbour = r_connect(u->region, direction);
    if(neighbour != NULL) {
      int dist = distance(neighbour, closest_phoenix->region);
      if(dist < closest_neighbour_distance) {
        closest_neighbour_direction = direction;
        closest_neighbour_distance = dist;
      } else if(dist == closest_neighbour_distance && rand()%100 < 50) {
        /* there can never be more than two neighbours with the same
         * distance (except when you are standing in the same region
         * as the phoenix, but that case has already been handled).
         * therefore this simple solution is correct */
        closest_neighbour_direction = direction;
        closest_neighbour_distance = dist;
      }
    }
  }

  add_message(&u->faction->msgs, msg_message("phoenixcompass_success",
        "unit region command dir",
        u, u->region, ord, closest_neighbour_direction));

  return 0;
}

void
register_phoenixcompass(void)
{
  register_function((pf_generic)use_phoenixcompass, "use_phoenixcompass");
}

