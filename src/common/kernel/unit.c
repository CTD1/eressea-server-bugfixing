/* vi: set ts=2:
*
*  
*  Eressea PB(E)M host Copyright (C) 1998-2003
*      Christian Schlittchen (corwin@amber.kn-bremen.de)
*      Katja Zedel (katze@felidae.kn-bremen.de)
*      Henning Peters (faroul@beyond.kn-bremen.de)
*      Enno Rehling (enno@eressea.de)
*      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
*
*  based on:
*
* Atlantis v1.0  13 September 1993 Copyright 1993 by Russell Wallace
* Atlantis v1.7                    Copyright 1996 by Alex Schr�der
*
* This program may not be used, modified or distributed without
* prior permission by the authors of Eressea.
* This program may not be sold or used commercially without prior written
* permission from the authors.
*/

#include <config.h>
#include <kernel/eressea.h>
#include "unit.h"

#include "building.h"
#include "faction.h"
#include "group.h"
#include "karma.h"
#include "connection.h"
#include "item.h"
#include "move.h"
#include "order.h"
#include "plane.h"
#include "race.h"
#include "region.h"
#include "save.h"
#include "ship.h"
#include "skill.h"
#include "terrain.h"

#include <attributes/moved.h>
#include <attributes/otherfaction.h>
#include <attributes/racename.h>

/* util includes */
#include <util/attrib.h>
#include <util/base36.h>
#include <util/event.h>
#include <util/goodies.h>
#include <util/language.h>
#include <util/lists.h>
#include <util/log.h>
#include <util/resolve.h>
#include <util/rng.h>
#include <util/storage.h>
#include <util/variant.h>

/* libc includes */
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#define FIND_FOREIGN_TEMP

attrib_type at_creator = {
  "creator"
  /* Rest ist NULL; tempor�res, nicht alterndes Attribut */
};

#define UMAXHASH MAXUNITS
static unit * unithash[UMAXHASH];
static unit * delmarker = (unit*)unithash; /* a funny hack */

#define HASH_STATISTICS 1
#if HASH_STATISTICS
static int hash_requests;
static int hash_misses;
#endif

void
uhash(unit * u)
{
  int key = HASH1(u->no, UMAXHASH), gk = HASH2(u->no, UMAXHASH);
  while (unithash[key]!=NULL && unithash[key]!=delmarker && unithash[key]!=u) {
    key = (key + gk) % UMAXHASH;
  }
  assert(unithash[key]!=u || !"trying to add the same unit twice");
  unithash[key] = u;
}

void
uunhash(unit * u)
{
  int key = HASH1(u->no, UMAXHASH), gk = HASH2(u->no, UMAXHASH);
  while (unithash[key]!=NULL && unithash[key]!=u) {
    key = (key + gk) % UMAXHASH;
  }
  assert(unithash[key]==u || !"trying to remove a unit that is not hashed");
  unithash[key] = delmarker;
}

unit *
ufindhash(int uid)
{
  assert(uid>=0);
#if HASH_STATISTICS
  ++hash_requests;
#endif
  if (uid>=0) {
    int key = HASH1(uid, UMAXHASH), gk = HASH2(uid, UMAXHASH);
    while (unithash[key]!=NULL && (unithash[key]==delmarker || unithash[key]->no!=uid)) {
      key = (key + gk) % UMAXHASH;
#if HASH_STATISTICS
      ++hash_misses;
#endif
    }
    return unithash[key];
  }
  return NULL;
}

#define DMAXHASH 7919
typedef struct dead {
  struct dead * nexthash;
  faction * f;
  int no;
} dead;

static dead* deadhash[DMAXHASH];

static void
dhash(int no, faction * f)
{
  dead * hash = (dead*)calloc(1, sizeof(dead));
  dead * old = deadhash[no % DMAXHASH];
  hash->no = no;
  hash->f = f;
  deadhash[no % DMAXHASH] = hash;
  hash->nexthash = old;
}

faction *
dfindhash(int no)
{
  dead * old;

  if(no < 0) return 0;

  for (old = deadhash[no % DMAXHASH]; old; old = old->nexthash) {
    if (old->no == no) {
      return old->f;
    }
  }
  return 0;
}

typedef struct friend {
  struct friend * next;
  int number;
  faction * faction;
  unit * unit;
} friend;

static friend *
get_friends(const unit * u, int * numfriends)
{
  friend * friends = 0;
  faction * f = u->faction;
  region * r = u->region;
  int number = 0;
  unit * u2;

  for (u2=r->units;u2;u2=u2->next) {
    if (u2->faction!=f && u2->number>0) {
      int allied = 0;
      if (get_param_int(global.parameters, "rules.alliances", 0)!=0) {
        allied = (f->alliance && f->alliance==u2->faction->alliance);
      }
      else if (alliedunit(u, u2->faction, HELP_MONEY) && alliedunit(u2, f, HELP_GIVE)) {
        allied = 1;
      }
      if (allied) {
        friend * nf, ** fr = &friends;

        /* some units won't take stuff: */
        if (u2->race->ec_flags & GETITEM) {
          while (*fr && (*fr)->faction->no<u2->faction->no) fr = &(*fr)->next;
          nf = *fr;
          if (nf==NULL || nf->faction!=u2->faction) {
            nf = malloc(sizeof(friend));
            nf->next = *fr;
            nf->faction = u2->faction;
            nf->unit = u2;
            nf->number = 0;
            *fr = nf;
          } else if (nf->faction==u2->faction && (u2->race->ec_flags & GIVEITEM)) {
              /* we don't like to gift it to units that won't give it back */
            if ((nf->unit->race->ec_flags & GIVEITEM) == 0) {
              nf->unit = u2;
            }
          }
          nf->number += u2->number;
          number += u2->number;
        }
      }
    }
  }
  if (numfriends) *numfriends = number;
  return friends;
}

/** give all items to friends or peasants.
 * this function returns 0 on success, or 1 if there are items that
 * could not be destroyed.
 */
int
gift_items(unit * u, int flags)
{
  region * r = u->region;
  item ** itm_p = &u->items;
  int retval = 0;
  int rule = rule_give();

  if ((u->faction->flags&FFL_QUIT)==0 || (rule&GIVE_ONDEATH)==0) {
    if ((rule&GIVE_ALLITEMS)==0 && (flags&GIFT_FRIENDS)) flags-=GIFT_FRIENDS;
    if ((rule&GIVE_PEASANTS)==0 && (flags&GIFT_PEASANTS)) flags-=GIFT_PEASANTS;
    if ((rule&GIVE_SELF)==0 && (flags&GIFT_SELF)) flags-=GIFT_SELF;
  }

  if (u->items==NULL || fval(u->race, RCF_ILLUSIONARY)) return 0;
  if ((u->race->ec_flags & GIVEITEM) == 0) return 0;

  /* at first, I should try giving my crap to my own units in this region */
  if (u->faction && (u->faction->flags&FFL_QUIT)==0 && (flags & GIFT_SELF)) {
    unit * u2, * u3 = NULL;
    for (u2 = r->units; u2; u2 = u2->next) {
      if (u2 != u && u2->faction == u->faction && u2->number>0) {
        /* some units won't take stuff: */
        if (u2->race->ec_flags & GETITEM) {
          /* we don't like to gift it to units that won't give it back */
          if (u2->race->ec_flags & GIVEITEM) {
            i_merge(&u2->items, &u->items);
            u->items = NULL;
            break;
          } else {
            u3 = u2;
          }
        }
      }
    }
    if (u->items && u3) {
      /* if nobody else takes it, we give it to a unit that has issues */
      i_merge(&u3->items, &u->items);
      u->items = NULL;
    }
    if (u->items==NULL) return 0;
  }

  /* if I have friends, I'll try to give my stuff to them */
  if (u->faction && (flags & GIFT_FRIENDS)) {
    int number = 0;
    friend * friends = get_friends(u, &number);

    while (friends) {
      struct friend * nf = friends;
      unit * u2 = nf->unit;
      item * itm = u->items;
      while (itm!=NULL) {
        const item_type * itype = itm->type;
        item * itn = itm->next;
        int n = itm->number;
        n = n * nf->number / number;
        if (n>0) {
          i_change(&u->items, itype, -n);
          i_change(&u2->items, itype, n);
        }
        itm = itn;
      }
      number -= nf->number;
      friends = nf->next;
      free(nf);
    }
    if (u->items==NULL) return 0;
  }

  /* last, but not least, give money and horses to peasants */
  while (*itm_p) {
    item * itm = *itm_p;

    if (flags & GIFT_PEASANTS) {
      if (!fval(u->region->terrain, SEA_REGION)) {
        if (itm->type==olditemtype[I_HORSE]) {
          rsethorses(r, rhorses(r) + itm->number);
          itm->number = 0;
        } else if (itm->type==i_silver) {
          rsetmoney(r, rmoney(r) + itm->number);
          itm->number = 0;
        }
      }
    }
    if (itm->number>0 && (itm->type->flags & ITF_NOTLOST)) {
      itm_p = &itm->next;
      retval = -1;
    } else {
      i_remove(itm_p, itm);
      i_free(itm);
    }
  }
  return retval;
}

void
make_zombie(unit * u)
{
  u_setfaction(u, get_monsters());
  scale_number(u, 1);
  u->race = new_race[RC_ZOMBIE];
  u->irace = NULL;
}

/** remove the unit from the list of active units.
 * the unit is not actually freed, because there may still be references
 * dangling to it (from messages, for example). To free all removed units, 
 * call free_units().
 * returns 0 on success, or -1 if unit could not be removed.
 */

static unit * deleted_units = NULL;

int
remove_unit(unit ** ulist, unit * u)
{
  int result;

  assert(ufindhash(u->no));
  handle_event(u->attribs, "destroy", u);

  result = gift_items(u, GIFT_SELF|GIFT_FRIENDS|GIFT_PEASANTS);
  if (result!=0) {
    make_zombie(u);
    return -1;
  }
  
  if (u->number) set_number(u, 0);
  leave(u, true);
  u->region = NULL;

  uunhash(u);
  if (ulist) {
    while (*ulist!=u) {
      ulist = &(*ulist)->next;
    }
    assert(*ulist==u);
    *ulist = u->next;
  }

  u->next = deleted_units;
  deleted_units = u;
  dhash(u->no, u->faction);

  u_setfaction(u, NULL);
  u->region = NULL;

  return 0;
}

unit *
findnewunit (const region * r, const faction *f, int n)
{
  unit *u2;

  if (n == 0)
    return 0;

  for (u2 = r->units; u2; u2 = u2->next)
    if (u2->faction == f && ualias(u2) == n)
      return u2;
#ifdef FIND_FOREIGN_TEMP
  for (u2 = r->units; u2; u2 = u2->next)
    if (ualias(u2) == n)
      return u2;
#endif
  return 0;
}

/* ------------------------------------------------------------- */


/*********************/
/*   at_alias   */
/*********************/
attrib_type at_alias = {
  "alias",
  DEFAULT_INIT,
  DEFAULT_FINALIZE,
  DEFAULT_AGE,
  NO_WRITE,
  NO_READ
};

int
ualias(const unit * u)
{
  attrib * a = a_find(u->attribs, &at_alias);
  if (!a) return 0;
  return a->data.i;
}

int
a_readprivate(attrib * a, struct storage * store)
{
  a->data.v = store->r_str(store);
  if (a->data.v) return AT_READ_OK;
  return AT_READ_FAIL;
}

/*********************/
/*   at_private   */
/*********************/
attrib_type at_private = {
  "private",
  DEFAULT_INIT,
  a_finalizestring,
  DEFAULT_AGE,
  a_writestring,
  a_readprivate
};

const char *
u_description(const unit * u, const struct locale * lang)
{
  if (u->display && u->display[0]) {
    return u->display;
  } else if (u->race->describe) {
    return u->race->describe(u, lang);
  }
  return NULL;
}

const char *
uprivate(const unit * u)
{
  attrib * a = a_find(u->attribs, &at_private);
  if (!a) return NULL;
  return a->data.v;
}

void
usetprivate(unit * u, const char * str)
{
  attrib * a = a_find(u->attribs, &at_private);

  if (str == NULL) {
    if (a) a_remove(&u->attribs, a);
    return;
  }
  if (!a) a = a_add(&u->attribs, a_new(&at_private));
  if (a->data.v) free(a->data.v);
  a->data.v = strdup((const char*)str);
}

/*********************/
/*   at_potionuser   */
/*********************/
/* Einheit BENUTZT einen Trank */
attrib_type at_potionuser = {
  "potionuser",
  DEFAULT_INIT,
  DEFAULT_FINALIZE,
  DEFAULT_AGE,
  NO_WRITE,
  NO_READ
};

void
usetpotionuse(unit * u, const potion_type * ptype)
{
  attrib * a = a_find(u->attribs, &at_potionuser);
  if (!a) a = a_add(&u->attribs, a_new(&at_potionuser));
  a->data.v = (void*)ptype;
}

const potion_type *
ugetpotionuse(const unit * u) {
  attrib * a = a_find(u->attribs, &at_potionuser);
  if (!a) return NULL;
  return (const potion_type *)a->data.v;
}

/*********************/
/*   at_target   */
/*********************/
attrib_type at_target = {
  "target",
  DEFAULT_INIT,
  DEFAULT_FINALIZE,
  DEFAULT_AGE,
  NO_WRITE,
  NO_READ
};

unit *
utarget(const unit * u) {
  attrib * a;
  if (!fval(u, UFL_TARGET)) return NULL;
  a = a_find(u->attribs, &at_target);
  assert (a || !"flag set, but no target found");
  return (unit*)a->data.v;
}

void
usettarget(unit * u, const unit * t)
{
  attrib * a = a_find(u->attribs, &at_target);
  if (!a && t) a = a_add(&u->attribs, a_new(&at_target));
  if (a) {
    if (!t) {
      a_remove(&u->attribs, a);
      freset(u, UFL_TARGET);
    }
    else {
      a->data.v = (void*)t;
      fset(u, UFL_TARGET);
    }
  }
}

/*********************/
/*   at_siege   */
/*********************/

void
a_writesiege(const attrib * a, struct storage * store)
{
  struct building * b = (struct building*)a->data.v;
  write_building_reference(b, store);
}

int
a_readsiege(attrib * a, struct storage * store)
{
  int result = read_reference(&a->data.v, store, read_building_reference, resolve_building);
  if (result==0 && !a->data.v) {
    return AT_READ_FAIL;
  }
  return AT_READ_OK;
}

attrib_type at_siege = {
  "siege",
  DEFAULT_INIT,
  DEFAULT_FINALIZE,
  DEFAULT_AGE,
  a_writesiege,
  a_readsiege
};

struct building *
  usiege(const unit * u) {
    attrib * a;
    if (!fval(u, UFL_SIEGE)) return NULL;
    a = a_find(u->attribs, &at_siege);
    assert (a || !"flag set, but no siege found");
    return (struct building *)a->data.v;
}

void
usetsiege(unit * u, const struct building * t)
{
  attrib * a = a_find(u->attribs, &at_siege);
  if (!a && t) a = a_add(&u->attribs, a_new(&at_siege));
  if (a) {
    if (!t) {
      a_remove(&u->attribs, a);
      freset(u, UFL_SIEGE);
    }
    else {
      a->data.v = (void*)t;
      fset(u, UFL_SIEGE);
    }
  }
}

/*********************/
/*   at_contact   */
/*********************/
attrib_type at_contact = {
  "contact",
  DEFAULT_INIT,
  DEFAULT_FINALIZE,
  DEFAULT_AGE,
  NO_WRITE,
  NO_READ
};

void
usetcontact(unit * u, const unit * u2)
{
  attrib * a = a_find(u->attribs, &at_contact);
  while (a && a->type==&at_contact && a->data.v!=u2) a = a->next;
  if (a && a->type==&at_contact) return;
  a_add(&u->attribs, a_new(&at_contact))->data.v = (void*)u2;
}

boolean
ucontact(const unit * u, const unit * u2)
/* Pr�ft, ob u den Kontaktiere-Befehl zu u2 gesetzt hat. */
{
  attrib *ru;
  if (u->faction==u2->faction) return true;

  /* Explizites KONTAKTIERE */
  for (ru = a_find(u->attribs, &at_contact); ru && ru->type==&at_contact; ru = ru->next) {
    if (((unit*)ru->data.v) == u2) {
      return true;
    }
  }

  return false;
}

/***
** init & cleanup module
**/

void
free_units(void)
{
  while (deleted_units) {
    unit * u = deleted_units;
    deleted_units = deleted_units->next;
    free_unit(u);
    free(u);
  }
}


void
write_unit_reference(const unit * u, struct storage * store)
{
  store->w_id(store, (u && u->region)?u->no:0);
}

int
resolve_unit(variant id, void * address)
{
  unit * u = NULL;
  if (id.i!=0) {
    u = findunit(id.i);
    if (u==NULL) {
      *(unit**)address = NULL;
      return -1;
    }
  }
  *(unit**)address = u;
  return 0;
}

variant
read_unit_reference(struct storage * store)
{
  variant var;
  var.i = store->r_id(store);
  return var;
}

attrib_type at_stealth = {
  "stealth", NULL, NULL, NULL, a_writeint, a_readint
};

void
u_seteffstealth(unit * u, int value)
{
  if (skill_enabled[SK_STEALTH]) {
    attrib * a = NULL;
    if (fval(u, UFL_STEALTH)) {
      a = a_find(u->attribs, &at_stealth);
    }
    if (value<0) {
      if (a!=NULL) {
        freset(u, UFL_STEALTH);
        a_remove(&u->attribs, a);
      }
      return;
    }
    if (a==NULL) {
      a = a_add(&u->attribs, a_new(&at_stealth));
      fset(u, UFL_STEALTH);
    }
    a->data.i = value;
  }
}

int
u_geteffstealth(const struct unit * u)
{
  if (skill_enabled[SK_STEALTH]) {
    if (fval(u, UFL_STEALTH)) {
      attrib * a = a_find(u->attribs, &at_stealth);
      if (a!=NULL) return a->data.i;
    }
  }
  return -1;
}

int
get_level(const unit * u, skill_t id)
{
  if (skill_enabled[id]) {
    skill * sv = u->skills;
    while (sv != u->skills + u->skill_size) {
      if (sv->id == id) {
        return sv->level;
      }
      ++sv;
    }
  }
  return 0;
}

void 
set_level(unit * u, skill_t sk, int value)
{
  skill * sv = u->skills;

  if (!skill_enabled[sk]) return;

  if (value==0) {
    remove_skill(u, sk);
    return;
  }
  while (sv != u->skills + u->skill_size) {
    if (sv->id == sk) {
      sk_set(sv, value);
      return;
    }
    ++sv;
  }
  sk_set(add_skill(u, sk), value);
}

static int  
leftship_age(struct attrib * a)
{
  /* must be aged, so it doesn't affect report generation (cansee) */
  unused(a);
  return AT_AGE_REMOVE; /* remove me */
}

static attrib_type at_leftship = {
  "leftship", NULL, NULL, leftship_age
};

static attrib *
make_leftship(struct ship * leftship)
{
  attrib * a = a_new(&at_leftship);
  a->data.v = leftship;
  return a;
}

void
set_leftship(unit *u, ship *sh)
{
  a_add(&u->attribs, make_leftship(sh));
}

ship *
leftship(const unit *u)
{
  attrib * a = a_find(u->attribs, &at_leftship);

  /* Achtung: Es ist nicht garantiert, da� der R�ckgabewert zu jedem
  * Zeitpunkt noch auf ein existierendes Schiff zeigt! */

  if (a) return (ship *)(a->data.v);

  return NULL;
}

void
leave_ship(unit * u)
{
  struct ship * sh = u->ship;
  if (sh==NULL) return;
  u->ship = NULL;
  set_leftship(u, sh);

  if (fval(u, UFL_OWNER)) {
    unit *u2, *owner = NULL;
    freset(u, UFL_OWNER);

    for (u2 = u->region->units; u2; u2 = u2->next) {
      if (u2->ship == sh) {
        if (u2->faction == u->faction) {
          owner = u2;
          break;
        } 
        else if (owner==NULL) owner = u2;
      }
    }
    if (owner!=NULL) fset(owner, UFL_OWNER);
  }
}

void
leave_building(unit * u)
{
  struct building * b = u->building;
  if (!b) return;
  u->building = NULL;

  if (fval(u, UFL_OWNER)) {
    unit *u2, *owner = NULL;
    freset(u, UFL_OWNER);

    for (u2 = u->region->units; u2; u2 = u2->next) {
      if (u2->building == b) {
        if (u2->faction == u->faction) {
          owner = u2;
          break;
        } 
        else if (owner==NULL) owner = u2;
      }
    }
    if (owner!=NULL) fset(owner, UFL_OWNER);
  }
}

boolean
can_leave(unit * u)
{
  static int rule_leave = -1;

  if (!u->building) {
    return true;
  }
  if (rule_leave<0) {
    rule_leave = get_param_int(global.parameters, "rules.move.owner_leave", 0);
  }
  if (rule_leave && u->building && u==building_owner(u->building)) {
    return false;
  }
  return true;
}

boolean
leave(unit * u, boolean force)
{
  if (!force) {
    if (!can_leave(u)) {
      return false;
    }
  }
  if (u->building) leave_building(u);
  else if (u->ship) leave_ship(u);
  return true;
}

const struct race * 
urace(const struct unit * u)
{
  return u->race;
}

boolean
can_survive(const unit *u, const region *r)
{
  if ((fval(r->terrain, WALK_INTO) && (u->race->flags & RCF_WALK))
    || (fval(r->terrain, SWIM_INTO) && (u->race->flags & RCF_SWIM)) 
    || (fval(r->terrain, FLY_INTO) && (u->race->flags & RCF_FLY))) 
  {
    static const curse_type * ctype = NULL;

    if (has_horses(u) && !fval(r->terrain, WALK_INTO))
      return false;

    if (!ctype) ctype = ct_find("holyground");
    if (fval(u->race, RCF_UNDEAD) && curse_active(get_curse(r->attribs, ctype)))
      return false;

    return true;
  }
  return false;
}

void
move_unit(unit * u, region * r, unit ** ulist)
{
  int maxhp = 0;
  assert(u && r);

  if (u->region == r) return;
  if (u->region!=NULL) maxhp = unit_max_hp(u);
  if (!ulist) ulist = (&r->units);
  if (u->region) {
    setguard(u, GUARD_NONE);
    fset(u, UFL_MOVED);
    if (u->ship || u->building) {
      /* can_leave must be checked in travel_i */
      boolean result = leave(u, false);
      assert(result);
    }
    translist(&u->region->units, ulist, u);
  } else {
    addlist(ulist, u);
  }

#ifdef SMART_INTERVALS
  update_interval(u->faction, r);
#endif
  u->region = r;
  /* keine automatische hp reduzierung bei bewegung */
  /* if (maxhp>0) u->hp = u->hp * unit_max_hp(u) / maxhp; */
}

/* ist mist, aber wegen nicht skalierender attribute notwendig: */
#include "alchemy.h"

void
transfermen(unit * u, unit * u2, int n)
{
  const attrib * a;
  int hp = u->hp;
  region * r = u->region;

  if (n==0) return;
  assert(n > 0);
  /* "hat attackiert"-status wird �bergeben */

  if (u2) {
    skill *sv, *sn;
    skill_t sk;
    ship * sh;

    assert(u2->number+n>0);

    for (sk=0; sk!=MAXSKILLS; ++sk) {
      int weeks, level = 0;

      sv = get_skill(u, sk);
      sn = get_skill(u2, sk);

      if (sv==NULL && sn==NULL) continue;
      if (sn==NULL && u2->number==0) {
        /* new unit, easy to solve */
        level = sv->level;
        weeks = sv->weeks;
      } else {
        double dlevel = 0.0;

        if (sv && sv->level) {
          dlevel += (sv->level + 1 - sv->weeks/(sv->level+1.0)) * n;
          level += sv->level * n;
        }
        if (sn && sn->level) {
          dlevel += (sn->level + 1 - sn->weeks/(sn->level+1.0)) * u2->number;
          level += sn->level * u2->number;
        }

        dlevel = dlevel / (n + u2->number);
        level = level / (n + u2->number);
        if (level<=dlevel) {
          /* apply the remaining fraction to the number of weeks to go.
          * subtract the according number of weeks, getting closer to the
          * next level */
          level = (int)dlevel;
          weeks = (level+1) - (int)((dlevel - level) * (level+1));
        } else {
          /* make it harder to reach the next level. 
          * weeks+level is the max difficulty, 1 - the fraction between
          * level and dlevel applied to the number of weeks between this
          * and the previous level is the added difficutly */
          level = (int)dlevel+1;
          weeks = 1 + 2 * level - (int)((1 + dlevel - level) * level);
        }
      }
      if (level) {
        if (sn==NULL) sn = add_skill(u2, sk);
        sn->level = (unsigned char)level;
        sn->weeks = (unsigned char)weeks;
        assert(sn->weeks>0 && sn->weeks<=sn->level*2+1);
        assert(u2->number!=0 || (sn->level==sv->level && sn->weeks==sv->weeks));
      } else if (sn) {
        remove_skill(u2, sk);
        sn = NULL;
      }
    }
    a = a_find(u->attribs, &at_effect);
    while (a && a->type==&at_effect) {
      effect_data * olde = (effect_data*)a->data.v;
      if (olde->value) change_effect(u2, olde->type, olde->value);
      a = a->next;
    }
    sh = leftship(u);
    if (sh!=NULL) set_leftship(u2, sh);
    u2->flags |= u->flags&(UFL_LONGACTION|UFL_NOTMOVING|UFL_HUNGER|UFL_MOVED|UFL_ENTER);
    if (u->attribs) {
      transfer_curse(u, u2, n);
    }
  }
  scale_number(u, u->number - n);
  if (u2) {
    set_number(u2, u2->number + n);
    hp -= u->hp;
    u2->hp += hp;
    /* TODO: Das ist schnarchlahm! und geh�rt nicht hierhin */
    a = a_find(u2->attribs, &at_effect);
    while (a && a->type==&at_effect) {
      attrib * an = a->next;
      effect_data * olde = (effect_data*)a->data.v;
      int e = get_effect(u, olde->type);
      if (e!=0) change_effect(u2, olde->type, -e);
      a = an;
    }
  }
  else if (r->land) {
    if ((u->race->ec_flags & ECF_REC_ETHEREAL)==0) {
      const race * rc = u->race;
      if (rc->ec_flags & ECF_REC_HORSES) { /* Zentauren an die Pferde */
        int h = rhorses(r) + n;
        rsethorses(r, h);
      } else {
        int p = rpeasants(r);
        p += (int)(n * rc->recruit_multi);
        rsetpeasants(r, p);
      }
    }
  }
}

struct building * 
  inside_building(const struct unit * u)
{
  if (u->building==NULL) return NULL;

  if (!fval(u->building, BLD_WORKING)) {
    /* Unterhalt nicht bezahlt */
    return NULL;
  } else if (u->building->size < u->building->type->maxsize) {
    /* Geb�ude noch nicht fertig */
    return NULL;
  } else {
    int p = 0, cap = buildingcapacity(u->building);
    const unit * u2;
    for (u2 = u->region->units; u2; u2 = u2->next) {
      if (u2->building == u->building) {
        p += u2->number;
        if (u2 == u) {
          if (p <= cap) return u->building;
          return NULL;
        }
        if (p > cap) return NULL;
      }
    }  
  }
  return NULL;
}

void
u_setfaction(unit * u, faction * f)
{
  int cnt = u->number;

  if (u->faction==f) return;
  if (u->faction) {
    set_number(u, 0);
    if (count_unit(u)) --u->faction->no_units;
    join_group(u, NULL);
    free_orders(&u->orders);
    set_order(&u->thisorder, NULL);

    if (u->nextF) u->nextF->prevF = u->prevF;
    if (u->prevF) u->prevF->nextF = u->nextF;
    else u->faction->units = u->nextF;
  }

  if (f!=NULL) {
    if (f->units) f->units->prevF=u;
    u->prevF = NULL;
    u->nextF = f->units;
    f->units = u;
  }
  else u->nextF = NULL;

  u->faction = f;
  if (u->region) update_interval(f, u->region);
  if (cnt && f) {
    set_number(u, cnt);
    if (count_unit(u)) ++f->no_units;
  }
}

/* vorsicht Spr�che k�nnen u->number == RS_FARVISION haben! */
void
set_number(unit * u, int count)
{
  assert (count >= 0);
  assert (count <= UNIT_MAXSIZE);

#ifndef NDEBUG
  assert(u->faction || count==0);
#endif

  if (u->faction) {
    if (playerrace(u->race)) {
      u->faction->num_people += count - u->number;
    }
    u->number = (unsigned short)count;
  } else if (u->number>0) {
    assert(!"why doesn't this unit have a faction? this will fuck up num_people");
  }
}

boolean
learn_skill(unit * u, skill_t sk, double chance)
{
  skill * sv = u->skills;
  if (chance < 1.0 && rng_int()%10000>=chance*10000) return false;
  while (sv != u->skills + u->skill_size) {
    assert (sv->weeks>0);
    if (sv->id == sk) {
      if (sv->weeks<=1) {
        sk_set(sv, sv->level+1);
      } else {
        sv->weeks--;
      }
      return true;
    }
    ++sv;
  }
  sv = add_skill(u, sk);
  sk_set(sv, 1);
  return true;
}

void
remove_skill(unit *u, skill_t sk)
{
  skill * sv = u->skills;
  for (sv = u->skills; sv != u->skills + u->skill_size; ++sv) {
    if (sv->id==sk) {
      skill * sl = u->skills + u->skill_size - 1;
      if (sl!=sv) {
        *sv = *sl;
      }
      --u->skill_size;
      return;
    }
  }
}

skill * 
add_skill(unit * u, skill_t id)
{
  skill * sv = u->skills;
#ifndef NDEBUG
  for (sv = u->skills; sv != u->skills + u->skill_size; ++sv) {
    assert(sv->id != id);
  }
#endif
  ++u->skill_size;
  u->skills = realloc(u->skills, u->skill_size * sizeof(skill));
  sv = (u->skills + u->skill_size - 1);
  sv->level = (unsigned char)0;
  sv->weeks = (unsigned char)1;
  sv->old   = (unsigned char)0;
  sv->id    = (unsigned char)id;
  return sv;
}

skill *
get_skill(const unit * u, skill_t sk)
{
  skill * sv = u->skills;
  while (sv!=u->skills+u->skill_size) {
    if (sv->id==sk) return sv;
    ++sv;
  }
  return NULL;
}

boolean
has_skill(const unit * u, skill_t sk)
{
  skill * sv = u->skills;
  while (sv!=u->skills+u->skill_size) {
    if (sv->id==sk) {
      return (sv->level>0);
    }
    ++sv;
  }
  return false;
}

static int
item_modification(const unit *u, skill_t sk, int val)
{
  /* Presseausweis: *2 Spionage, 0 Tarnung */
  if(sk == SK_SPY && get_item(u, I_PRESSCARD) >= u->number) {
    val = val * 2;
  } else if(sk == SK_STEALTH) {
#if NEWATSROI == 1
    if (get_item(u, I_RING_OF_INVISIBILITY)
      + 100 * get_item(u, I_SPHERE_OF_INVISIBILITY) >= u->number) {
        val += ROIBONUS;
    }
#endif
    if(get_item(u, I_PRESSCARD) >= u->number) {
      val = 0;
    }
  }
#if NEWATSROI == 1
  if(sk == SK_PERCEPTION) {
    if(get_item(u, I_AMULET_OF_TRUE_SEEING) >= u->number) {
      val += ATSBONUS;
    }
  }
#endif
  return val;
}

static int
att_modification(const unit *u, skill_t sk)
{
  double bonus = 0, malus = 0;
  attrib * a;
  double result = 0;
  static boolean init = false;
  static const curse_type * skillmod_ct, * gbdream_ct, * worse_ct;
  curse * c;

  if (!init) { 
    init = true; 
    skillmod_ct = ct_find("skillmod"); 
    gbdream_ct = ct_find("gbdream");
    worse_ct = ct_find("worse");
  }

  c = get_curse(u->attribs, worse_ct);
  if (c!=NULL) result += curse_geteffect(c);
  if (skillmod_ct) {
    attrib * a = a_find(u->attribs, &at_curse);
    while (a && a->type==&at_curse) {
      curse * c = (curse *)a->data.v;
      if (c->type==skillmod_ct && c->data.i==sk) {
        result += curse_geteffect(c);
        break;
      }
      a = a->next;
    }
  }

  /* TODO hier kann nicht mit get/iscursed gearbeitet werden, da nur der
  * jeweils erste vom Typ C_GBDREAM zur�ckgegen wird, wir aber alle
  * durchsuchen und aufaddieren m�ssen */
  a = a_find(u->region->attribs, &at_curse);
  while (a && a->type==&at_curse) {
    curse * c = (curse*)a->data.v;
    if (curse_active(c) && c->type==gbdream_ct) {
      double mod = curse_geteffect(c);
      unit * mage = c->magician;
      /* wir suchen jeweils den gr��ten Bonus und den gr��ten Malus */
      if (mod>bonus) {
        if (mage==NULL || mage->number==0 || alliedunit(mage, u->faction, HELP_GUARD)) {
          bonus = mod;
        }
      } else if (mod < malus) {
        if (mage == NULL || !alliedunit(mage, u->faction, HELP_GUARD)) {
          malus = mod;
        }
      }
    }
    a = a->next;
  }
  result = result + bonus + malus;

  return (int)result;
}

int
get_modifier(const unit *u, skill_t sk, int level, const region *r, boolean noitem)
{
  int bskill = level;
  int skill = bskill;

  assert(r);
  if (sk == SK_STEALTH) {
    plane * pl = rplane(r);
    if (pl &&fval(pl, PFL_NOSTEALTH)) return 0;
  }

  skill += rc_skillmod(u->race, r, sk);
  skill += att_modification(u, sk);

  if (noitem == false) {
    skill = item_modification(u, sk, skill);
  }
  skill = skillmod(u->attribs, u, r, sk, skill, SMF_ALWAYS);

#if KARMA_MODULE
  if (fspecial(u->faction, FS_TELEPATHY)) {
    switch(sk) {
case SK_ALCHEMY:
case SK_HERBALISM:
case SK_MAGIC:
case SK_SPY:
case SK_STEALTH:
case SK_PERCEPTION:
  break;
default:
  skill -= 2;
    }
  }
#endif

#ifdef HUNGER_REDUCES_SKILL
  if (fval(u, UFL_HUNGER)) {
    skill = skill/2;
  }
#endif
  return skill - bskill;
}

int
eff_skill(const unit * u, skill_t sk, const region * r)
{
  if (skill_enabled[sk]) {
    int level = get_level(u, sk);
    if (level>0) {
      int mlevel = level + get_modifier(u, sk, level, r, false);

      if (mlevel>0) {
        int skillcap = SkillCap(sk);
        if (skillcap && mlevel>skillcap) {
          return skillcap;
        }
        return mlevel;
      }
    }
  }
  return 0;
}

int
eff_skill_study(const unit * u, skill_t sk, const region * r)
{
  int level = get_level(u, sk);
  if (level>0) {
    int mlevel = level + get_modifier(u, sk, level, r, true);

    if (mlevel>0) return mlevel;
  }
  return 0;
}

int
invisible(const unit *target, const unit * viewer)
{
#if NEWATSROI == 1
  return 0;
#else
  if (viewer && viewer->faction==target->faction) return 0;
  else {
    int hidden = get_item(target, I_RING_OF_INVISIBILITY) + 100 * get_item(target, I_SPHERE_OF_INVISIBILITY);
    if (hidden) {
      hidden = MIN(hidden, target->number);
      if (viewer) hidden -= get_item(viewer, I_AMULET_OF_TRUE_SEEING);
    }
    return hidden;
  }
#endif
}

/** remove the unit from memory.
 * this frees all memory that's only accessible through the unit,
 * and you should already have called uunhash and removed the unit from the
 * region.
 */
void
free_unit(unit * u)
{
  free(u->name);
  free(u->display);
  free_order(u->thisorder);
  free_orders(&u->orders);
  if (u->skills) free(u->skills);
  while (u->items) {
    item * it = u->items->next;
    u->items->next = NULL;
    i_free(u->items);
    u->items = it;
  }
  while (u->attribs) a_remove (&u->attribs, u->attribs);
  while (u->reservations) {
    struct reservation *res = u->reservations;
    u->reservations = res->next;
    free(res);
  }
}


void 
unitlist_clear(struct unit_list **ul)
{
  while (*ul) {
    unit_list * rl2 = (*ul)->next;
    free(*ul);
    *ul = rl2;
  }
}

void 
unitlist_insert(struct unit_list **ul, struct unit *u)
{
  unit_list *rl2 = (unit_list*)malloc(sizeof(unit_list));

  rl2->data = u;
  rl2->next = *ul;

  *ul = rl2;
}


static void
createunitid(unit *u, int id)
{
  if (id<=0 || id > MAX_UNIT_NR || ufindhash(id) || dfindhash(id) || forbiddenid(id))
    u->no = newunitid();
  else
    u->no = id;
  uhash(u);
}

void
name_unit(unit *u)
{
  if (u->race->generate_name) {
    const char * gen_name = u->race->generate_name(u); 
    if (gen_name) {
      unit_setname(u, gen_name);
    } else {
      unit_setname(u, racename(u->faction->locale, u, u->race));
    }
  } else {
    char name[16];
    sprintf(name, "%s %s", LOC(u->faction->locale, "unitdefault"), itoa36(u->no));
    unit_setname(u, name);
  }
}

/** creates a new unit.
*
* @param dname: name, set to NULL to get a default.
* @param creator: unit to inherit stealth, group, building, ship, etc. from
*/
unit *
create_unit(region * r, faction * f, int number, const struct race *urace, int id, const char * dname, unit *creator)
{
  unit * u = calloc(1, sizeof(unit));
  order * deford = default_order(f->locale);

  assert(urace);
  assert(deford);
  assert(f->alive);
  u_setfaction(u, f);
  set_order(&u->thisorder, NULL);
  addlist(&u->orders, deford);
  u_seteffstealth(u, -1);
  u->race = urace;
  u->irace = NULL;

  set_number(u, number);

  /* die nummer der neuen einheit muss vor name_unit generiert werden,
  * da der default name immer noch 'Nummer u->no' ist */
  createunitid(u, id);

  /* zuerst in die Region setzen, da zb Drachennamen den Regionsnamen
  * enthalten */
  if (r) move_unit(u, r, NULL);

  /* u->race muss bereits gesetzt sein, wird f�r default-hp gebraucht */
  /* u->region auch */
  u->hp = unit_max_hp(u) * number;

  if (!dname) {
    name_unit(u);
  } else {
    u->name = strdup(dname);
  }
  if (count_unit(u)) f->no_units++;

  if (creator) {
    attrib * a;

    /* erbt Kampfstatus */
    setstatus(u, creator->status);

    /* erbt Geb�ude/Schiff*/
    if (creator->region==r) {
      u->building = creator->building;
      if (creator->ship!=NULL && fval(u->race, RCF_CANSAIL)) {
        u->ship = creator->ship;
      }
    }

    /* Tarnlimit wird vererbt */
    if (fval(creator, UFL_STEALTH)) {
      attrib * a = a_find(creator->attribs, &at_stealth);
      if (a) {
        int stealth = a->data.i;
        a = a_add(&u->attribs, a_new(&at_stealth));
        a->data.i = stealth;
      }
    }

    /* Temps von parteigetarnten Einheiten sind wieder parteigetarnt */
    if (fval(creator, UFL_ANON_FACTION)) {
      fset(u, UFL_ANON_FACTION);
    }
    /* Daemonentarnung */
    set_racename(&u->attribs, get_racename(creator->attribs));
    if (fval(u->race, RCF_SHAPESHIFT) && fval(creator->race, RCF_SHAPESHIFT)) {
      u->irace = creator->irace;
    }

    /* Gruppen */
    if (creator->faction==f && fval(creator, UFL_GROUP)) {
      a = a_find(creator->attribs, &at_group);
      if (a) {
        group * g = (group*)a->data.v;
        set_group(u, g);
      }
    }
    a = a_find(creator->attribs, &at_otherfaction);
    if (a) {
      a_add(&u->attribs, make_otherfaction(get_otherfaction(a)));
    }

    a = a_add(&u->attribs, a_new(&at_creator));
    a->data.v = creator;
  }

  return u;
}

int 
maxheroes(const struct faction * f)
{
  int nsize = count_all(f);
  if (nsize==0) return 0;
  else {
    int nmax = (int)(log10(nsize / 50.0) * 20);
    return (nmax<0)?0:nmax;
  }
}

int 
countheroes(const struct faction * f)
{
  const unit * u = f->units;
  int n = 0;

  while (u) {
    if (fval(u, UFL_HERO)) n+= u->number;
    u = u->nextF;
  }
#ifdef DEBUG_MAXHEROES
  int m = maxheroes(f);
  if (n>m) {
    log_warning(("%s has %d of %d heroes\n", factionname(f), n, m));
  }
#endif
  return n;
}

const char *
unit_getname(const unit * u)
{
  return (const char *)u->name;
}

void
unit_setname(unit * u, const char * name)
{
  free(u->name);
  if (name) u->name = strdup(name);
  else u->name = NULL;
}

const char *
unit_getinfo(const unit * u)
{
  return (const char *)u->display;
}

void
unit_setinfo(unit * u, const char * info)
{
  free(u->display);
  if (info) u->display = strdup(info);
  else u->display = NULL;
}

int
unit_getid(const unit * u)
{
  return u->no;
}

void
unit_setid(unit * u, int id)
{
  unit * nu = findunit(id);
  if (nu==NULL) {
    uunhash(u);
    u->no = id;
    uhash(u);
  }
}

int
unit_gethp(const unit * u)
{
  return u->hp;
}

void
unit_sethp(unit * u, int hp)
{
  u->hp = hp;
}

status_t
unit_getstatus(const unit * u)
{
  return u->status;
}

void
unit_setstatus(unit * u, status_t status)
{
  u->status = status;
}

int
unit_getweight(const unit * u)
{
  return weight(u);
}

int
unit_getcapacity(const unit * u)
{
  return walkingcapacity(u);
}

void
unit_addorder(unit * u, order * ord)
{
  order ** ordp = &u->orders;
  while (*ordp) ordp = &(*ordp)->next;
  *ordp = ord;
  u->faction->lastorders = turn;
}

int
unit_max_hp(const unit * u)
{
  static int rules_stamina = -1;
  int h;
  double p;
  static const curse_type * heal_ct = NULL;

  if (rules_stamina<0) {
    rules_stamina = get_param_int(global.parameters, "rules.stamina", STAMINA_AFFECTS_HP);
  }
  h = u->race->hitpoints;
  if (heal_ct==NULL) heal_ct = ct_find("healingzone");

  if (rules_stamina & 1) {
    p = pow(effskill(u, SK_STAMINA) / 2.0, 1.5) * 0.2;
    h += (int) (h * p + 0.5);
  }
#if KARMA_MODULE
  if (fspecial(u->faction, FS_UNDEAD)) {
    h *= 2;
  }
#endif /* KARMA_MODULE */

  /* der healing curse ver�ndert die maximalen hp */
  if (heal_ct) {
    curse *c = get_curse(u->region->attribs, heal_ct);
    if (c) {
      h = (int) (h * (1.0+(curse_geteffect(c)/100)));
    }
  }

  return h;
}

void
scale_number (unit * u, int n)
{
  skill_t sk;
  const attrib * a;
  int remain;

  if (n == u->number) return;
  if (n && u->number>0) {
    int full;
    remain = ((u->hp%u->number) * (n % u->number)) % u->number;

    full = u->hp/u->number; /* wieviel kriegt jede person mindestens */
    u->hp = full * n + (u->hp-full*u->number) * n / u->number;
    assert(u->hp>=0);
    if ((rng_int() % u->number) < remain)
      ++u->hp;  /* Nachkommastellen */
  } else {
    remain = 0;
    u->hp = 0;
  }
  if (u->number>0) {
    for (a = a_find(u->attribs, &at_effect);a && a->type==&at_effect;a=a->next) {
      effect_data * data = (effect_data *)a->data.v;
      int snew = data->value / u->number * n;
      if (n) {
        remain = data->value - snew / n * u->number;
        snew += remain * n / u->number;
        remain = (remain * n) % u->number;
        if ((rng_int() % u->number) < remain)
          ++snew; /* Nachkommastellen */
      }
      data->value = snew;
    }
  }
  if (u->number==0 || n==0) {
    for (sk = 0; sk < MAXSKILLS; sk++) {
      remove_skill(u, sk);
    }
  }

  set_number(u, n);
}

const struct race * u_irace(const struct unit * u)
{
  if (u->irace && skill_enabled[SK_STEALTH]) {
    return u->irace;
  }
  return u->race;
}
