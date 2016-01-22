/*
Copyright (c) 1998-2015, Enno Rehling <enno@eressea.de>
Katja Zedel <katze@felidae.kn-bremen.de
Christian Schlittchen <corwin@amber.kn-bremen.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**/

#include <platform.h>
#include <kernel/config.h>
#include <kernel/order.h>
#include "ship.h"

/* kernel includes */
#include "build.h"
#include "curse.h"
#include "faction.h"
#include "unit.h"
#include "item.h"
#include "keyword.h"
#include "move.h"
#include "race.h"
#include "region.h"
#include "skill.h"

/* util includes */
#include <util/attrib.h>
#include <util/base36.h>
#include <util/bsdstring.h>
#include <util/event.h>
#include <util/language.h>
#include <util/lists.h>
#include <util/umlaut.h>
#include <util/parser.h>
#include <quicklist.h>
#include <util/xml.h>

#include <attributes/movement.h>

#include <storage.h>

/* libc includes */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

quicklist *shiptypes = NULL;

static local_names *snames;

ship_type *findshiptype(const char *name, const struct locale *lang) /* CTD const */
{
    local_names *sn = snames;
    variant var;

    while (sn) {
        if (sn->lang == lang)
            break;
        sn = sn->next;
    }
    if (!sn) {
        quicklist *ql;
        int qi;

        sn = (local_names *)calloc(sizeof(local_names), 1);
        sn->next = snames;
        sn->lang = lang;

        for (qi = 0, ql = shiptypes; ql; ql_advance(&ql, &qi, 1)) {
            ship_type *stype = (ship_type *)ql_get(ql, qi);
            variant var2;
            const char *n = LOC(lang, stype->_name);
            var2.v = (void *)stype;
            addtoken(&sn->names, n, var2);
        }
        snames = sn;
    }
    if (findtoken(sn->names, name, &var) == E_TOK_NOMATCH)
        return NULL;
    return (ship_type *)var.v;  /* CTD const */
}

static ship_type *st_find_i(const char *name)
{
    quicklist *ql;
    int qi;

    for (qi = 0, ql = shiptypes; ql; ql_advance(&ql, &qi, 1)) {
        ship_type *stype = (ship_type *)ql_get(ql, qi);
        if (strcmp(stype->_name, name) == 0) {
            return stype;
        }
    }
    return NULL;
}

ship_type *st_find(const char *name) { /* CTD const */
    return st_find_i(name);
}

ship_type *st_get_or_create(const char * name) {
    ship_type * st = st_find_i(name);
    if (!st) {
        st = (ship_type *)calloc(sizeof(ship_type), 1);
        st->_name = _strdup(name);
        st->storm = 1.0;
        ql_push(&shiptypes, (void *)st);
    }
    return st;
}

#define MAXSHIPHASH 7919
ship *shiphash[MAXSHIPHASH];
void shash(ship * s)
{
    ship *old = shiphash[s->no % MAXSHIPHASH];

    shiphash[s->no % MAXSHIPHASH] = s;
    s->nexthash = old;
}

void sunhash(ship * s)
{
    ship **show;

    for (show = &shiphash[s->no % MAXSHIPHASH]; *show; show = &(*show)->nexthash) {
        if ((*show)->no == s->no)
            break;
    }
    if (*show) {
        assert(*show == s);
        *show = (*show)->nexthash;
        s->nexthash = 0;
    }
}

static ship *sfindhash(int i)
{
    ship *old;

    for (old = shiphash[i % MAXSHIPHASH]; old; old = old->nexthash)
        if (old->no == i)
            return old;
    return 0;
}

struct ship *findship(int i)
{
    return sfindhash(i);
}

struct ship *findshipr(const region * r, int n)
{
    ship *sh;

    for (sh = r->ships; sh; sh = sh->next) {
        if (sh->no == n) {
            assert(sh->region == r);
            return sh;
        }
    }
    return 0;
}

void damage_ship(ship * sh, double percent)
{
    double damage =
        DAMAGE_SCALE * sh->type->damage * percent * sh->size + sh->damage + .000001;
    sh->damage = (int)damage;
}

/* Alte Schiffstypen: */
static ship *deleted_ships;

ship *new_ship(ship_type * stype, region * r, const struct locale *lang) /* CTD const (const ship_type * stype, region * r, const struct locale *lang)*/
{
    static char buffer[32];
    ship *sh = (ship *)calloc(1, sizeof(ship));
    const char *sname = 0;

    assert(stype);
    sh->no = newcontainerid();
    sh->coast = NODIRECTION;
    sh->type = stype;
    sh->region = r;

    if (lang) {
        sname = LOC(lang, stype->_name);
        if (!sname) {
            sname = LOC(lang, parameters[P_SHIP]);
        }
    }
    if (!sname) {
        sname = parameters[P_SHIP];
    }
    assert(sname);
    slprintf(buffer, sizeof(buffer), "%s %s", sname, shipid(sh));
    sh->name = _strdup(buffer);
    shash(sh);
    if (r) {
        addlist(&r->ships, sh);
    }
    return sh;
}

void remove_ship(ship ** slist, ship * sh)
{
    region *r = sh->region;
    unit *u = r->units;

    handle_event(sh->attribs, "destroy", sh);
    while (u) {
        if (u->ship == sh) {
            leave_ship(u);
        }
        u = u->next;
    }
    sunhash(sh);
    while (*slist && *slist != sh)
        slist = &(*slist)->next;
    assert(*slist);
    *slist = sh->next;
    sh->next = deleted_ships;
    deleted_ships = sh;
    sh->region = NULL;
}

void free_ship(ship * s)
{
    while (s->attribs)
        a_remove(&s->attribs, s->attribs);
    free(s->name);
    free(s->display);
    free(s);
}

static void free_shiptype(void *ptr) {
    ship_type *stype = (ship_type *)ptr;
    free(stype->_name);
    free(stype->coasts);
    free_construction(stype->construction);
    free(stype);
}

void free_shiptypes(void) {
    ql_foreach(shiptypes, free_shiptype);
    ql_free(shiptypes);
    shiptypes = 0;
}

void free_ships(void)
{
    while (deleted_ships) {
        ship *s = deleted_ships;
        deleted_ships = s->next;
    }
}

const char *write_shipname(const ship * sh, char *ibuf, size_t size)
{
    slprintf(ibuf, size, "%s (%s)", sh->name, itoa36(sh->no));
    return ibuf;
}

static int ShipSpeedBonus(const unit * u)
{
    int level = config_get_int("movement.shipspeed.skillbonus", 0);
    if (level > 0) {
        ship *sh = u->ship;
        int skl = effskill(u, SK_SAILING, 0);
        int minsk = (sh->type->cptskill + 1) / 2;
        return (skl - minsk) / level;
    }
    return 0;
}

int crew_skill(const ship *sh) {
    int n = 0;
    unit *u;

    n = 0;

    for (u = sh->region->units; u; u = u->next) {
        if (u->ship == sh) {
            n += effskill(u, SK_SAILING, 0) * u->number;
        }
    }
    return n;
}

int shipspeed(ship * sh, const unit * u) /* CTD const (const ship * sh, const unit * u)*/
{
    assert(sh);

    int k = sh->type->range;
    static const struct curse_type *stormwind_ct, *nodrift_ct;
    static bool init;
    attrib *a;
    struct curse *c;
    int bonus;

    if (!u) u = ship_owner(sh);
    if (!u) return 0;
    if (sh->fleet){
        assert(u->ship == sh->fleet);
    }
    else
    {
        assert(u->ship == sh);
    }
    assert(u == ship_owner(sh));

    if (sh->type == st_find("fleet")) {
        fleetdamage_to_ships(sh);
        init_fleet(sh);
        k = sh->type->range; /* shipspeed for fleets is already init_fleet */
        return k;
    }

    assert(sh->type->construction);
    assert(sh->type->construction->improvement == NULL);  /* sonst ist construction::size nicht ship_type::maxsize */

    if (!init) {
        init = true;
        stormwind_ct = ct_find("stormwind");
        nodrift_ct = ct_find("nodrift");
    }
    if (sh->size != sh->type->construction->maxsize)
        return 0;

    if (curse_active(get_curse(sh->attribs, stormwind_ct)))
        k *= 2;
    if (curse_active(get_curse(sh->attribs, nodrift_ct)))
        k += 1;

    if (u->faction->race == u_race(u)) {
        /* race bonus for this faction? */
        if (fval(u_race(u), RCF_SHIPSPEED)) {
            k += 1;
        }
    }

    bonus = ShipSpeedBonus(u);
    if (bonus > 0 && sh->type->range_max>sh->type->range) {
        int crew = crew_skill(sh);
        int crew_bonus = (crew / sh->type->sumskill / 2) - 1;
        if (crew_bonus > 0) {
            bonus = _min(bonus, crew_bonus);
            bonus = _min(bonus, sh->type->range_max - sh->type->range);
        }
        else {
            bonus = 0;
        }
    }
    k += bonus;

    a = a_find(sh->attribs, &at_speedup);
    while (a != NULL && a->type == &at_speedup) {
        k += a->data.sa[0];
        a = a->next;
    }

    c = get_curse(sh->attribs, ct_find("shipspeedup"));
    while (c) {
        k += curse_geteffect_int(c);
        c = c->nexthash;
    }

    if (sh->damage>0) {
        int size = sh->size * DAMAGE_SCALE;
        k *= (size - sh->damage);
        k = (k + size - 1) / size;
    }
    return k;
}

const char *shipname(const ship * sh)
{
    typedef char name[OBJECTIDSIZE + 1];
    static name idbuf[8];
    static int nextbuf = 0;
    char *ibuf = idbuf[(++nextbuf) % 8];
    return write_shipname(sh, ibuf, sizeof(name));
}

int shipcapacity(const ship * sh)
{
    int i = sh->type->cargo;

    /* sonst ist construction:: size nicht ship_type::maxsize */
    assert(!sh->type->construction
        || sh->type->construction->improvement == NULL);

    if (sh->type->construction && sh->size != sh->type->construction->maxsize)
        return 0;

    if (sh->damage) {
        i = (int)ceil(i * (1.0 - sh->damage / sh->size / (double)DAMAGE_SCALE));
    }
    return i;
}

void getshipweight(const ship * sh, int *sweight, int *scabins)
{
    unit *u;

    *sweight = 0;
    *scabins = 0;

    for (u = sh->region->units; u; u = u->next) {
        if (u->ship == sh) {
            *sweight += weight(u);
            if (sh->type->cabins) {
                int pweight = u->number * u_race(u)->weight;
                /* weight goes into number of cabins, not cargo */
                *scabins += pweight;
                *sweight -= pweight;
            }
        }
    }
}

void ship_set_owner(unit * u) {
    assert(u && u->ship);
    u->ship->_owner = u;
}

static unit * ship_owner_ex(const ship * sh, const struct faction * last_owner)
{
    unit *u, *heir = 0;

    /* Eigentümer tot oder kein Eigentümer vorhanden. Erste lebende Einheit
      * nehmen. */
    for (u = sh->region->units; u; u = u->next) {
        if (u->ship == sh) {
            if (u->number > 0) {
                if (heir && last_owner && heir->faction != last_owner && u->faction == last_owner) {
                    heir = u;
                    break; /* we found someone from the same faction who is not dead. let's take this guy */
                }
                else if (!heir) {
                    heir = u; /* you'll do in an emergency */
                }
            }
        }
    }
    return heir;
}

void ship_update_owner(ship * sh) {
    unit * owner = sh->_owner;
    sh->_owner = ship_owner_ex(sh, owner ? owner->faction : 0);
}

unit *ship_owner(ship * sh) /*CTD const (const ship * sh)*/
{
    unit *owner = sh->_owner;
    if (sh->fleet) {
        owner = ship_owner(sh->fleet);
    }
    if (!owner || ((!sh->fleet && owner->ship != sh) || (sh->fleet && owner->ship != sh->fleet) || owner->number <= 0)) {
        unit * heir = ship_owner_ex(sh, owner ? owner->faction : 0);
        return (heir && heir->number > 0) ? heir : 0;
    }
    return owner;
}

void write_ship_reference(const struct ship *sh, struct storage *store)
{
    WRITE_INT(store, (sh && sh->region) ? sh->no : 0);
}

void ship_setname(ship * self, const char *name)
{
    free(self->name);
    self->name = name ? _strdup(name) : 0;
}

const char *ship_getname(const ship * self)
{
    return self->name;
}

int ship_damage_percent(const ship *ship) {
    return (ship->damage * 100 + DAMAGE_SCALE - 1) / (ship->size * DAMAGE_SCALE);
}


void fleet(region * r)
{
    unit **uptr;
    ship *sh;
    ship *fl;
    unit *u2;

    for (uptr = &r->units; *uptr;) {
        unit *u = *uptr;
        order **ordp = &u->orders;

        while (*ordp) {
            order *ord = *ordp;
            if (getkeyword(ord) == K_FLEET) {
                char token[128];
                param_t p;
                const char * s;

                init_order(ord);
                s = gettoken(token, sizeof(token));
                p = findparam_ex(s, u->faction->locale);

                // Minimum Segeln 6
                if (effskill(u, SK_SAILING, 0) < 6 || !(fval(u_race(u), RCF_CANSAIL))) {
                    // TODO Fecmistake(u, u->thisorder, 100, MSG_PRODUCE);
                    return;
                }
                /* Die Schiffsnummer bzw der Schiffstyp wird eingelesen */
                sh = getship(r);

                switch (p) {
                case P_CREATE:
                    if (!sh)
                        sh = u->ship;
                    if (!sh || sh->type == st_find("fleet")) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // Kein Schiff oder eine Flotte angegeben, nur einzelne Schiffe können in eine Flotte.
                        break;
                    }
                    // Wass wenn garkeiner auf dem Schiff ist??????
                    // Muss dringend gebrüft werden!! -> OK
                    if (ship_owner(sh) && !ucontact(u, ship_owner(sh))) {
                        /* Prueft, ob u den Kontaktiere-Befehl zum Schiffsbesitzer gesetzt ist */
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        break;
                    }

                    // aber nur wenn nicht schon Chef einer Flotte!
                    if (u->ship && u->ship->type == st_find("fleet")) {
                        if (ship_owner(u->ship) == u) {
                            // Error, Einheit ist schon Flottencheff cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                            break;
                        }
                    }

                    if (leave(u, true)) {
                        if (fval(u_race(u), RCF_CANSAIL)) {

                            // Flotte anlegen, Einheit wird Besitzer, Schiff hinzufügen, alle Einheiten aus dem Schiff der Flotte hinzufügen!
                            ship_type *newtype = st_find("fleet");
                            fl = new_ship(newtype, r, u->faction->locale);
                            u_set_ship(u, fl);
                            sh->_owner = u; // aber u ist auf der flotte, nicht auf dem Schiff! Sicherstellen das da nix schief geht! -> ship_owner
                            fl->size = 1;
                            sh->fleet = fl;
                            // u ans ende der Region sortieren??

                            // add to fleet

                            /* all units leave the ship and go to the fleet */
                            for (u2 = r->units; u2; u2 = u2->next) {
                                if (u2->ship == sh) {
                                    leave_ship(u2);
                                    u_set_ship(u2, u->ship);
                                    unit *ulast;
                                    unit *ub2;
                                    ulast = u2;
                                    for (ub2 = u2; ub2; ub2 = ub2->next) {
                                        if (ub2->ship == u2->ship) {
                                            ulast = ub2;
                                        }
                                    }
                                    if (ulast != u2) {
                                        /* put u2 behind ulast so it's the last unit in the ship */
                                        *uptr = u2->next;
                                        u2->next = ulast->next;
                                        ulast->next = u2;
                                    }
                                    if (*uptr == u2)
                                        uptr = &u2->next;
                                }
                            }
                            break;

                case P_JOIN:
                    fl = u->ship;
                    if (!fl || fl->type != st_find("fleet") || ship_owner(fl) != u) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // error only fleet kaptn can add ships!
                        break;
                    }

                    if (!sh || sh->type == st_find("fleet")) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // Kein Schiff oder eine Flotte angegeben, nur einzelne Schiffe können in eine Flotte.
                        break;
                    }

                    if (fl == sh->fleet) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // error ship is already in the fleet!
                        break;
                    }

                    sh->_owner = u; // aber u ist auf der flotte, nicht auf dem Schiff! Sicherstellen das da nix schief geht! -> ship_owner
                    fl->size++;
                    sh->fleet = fl;

                    // add to fleet

                    /* all units leave the ship and go to the fleet */
                    for (u2 = r->units; u2; u2 = u2->next) {
                        if (u2->ship == sh) {
                            leave_ship(u2);
                            u_set_ship(u2, u->ship);
                            unit *ulast;
                            unit *ub2;
                            ulast = u2;
                            for (ub2 = u2; ub2; ub2 = ub2->next) {
                                if (ub2->ship == u2->ship) {
                                    ulast = ub2;
                                }
                            }
                            if (ulast != u2) {
                                /* put u2 behind ulast so it's the last unit in the ship */
                                *uptr = u2->next;
                                u2->next = ulast->next;
                                ulast->next = u2;
                            }
                            if (*uptr == u2)
                                uptr = &u2->next;

                        }
                    }
                    break;
                case P_LEAVE:
                    fl = u->ship;
                    if (!fl || fl->type != st_find("fleet") || ship_owner(fl) != u) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // error only fleet kapt'n can add ships!
                        break;
                    }

                    if (!sh || sh->type == st_find("fleet")) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // Kein Schiff oder eine Flotte angegeben, nur einzelne Schiffe können die Flotte verlassen.
                        break;
                    }

                    if (fl != sh->fleet) {
                        // cmistake(u, u->thisorder, 20, MSG_PRODUCE);
                        // error ship is not part of the fleet!
                        break;
                    }

                    fl->size--;
                    sh->fleet = NULL;
                    sh->_owner = NULL;

                    if (fl->size < 1) {
                        // delet fleet!
                        // make sure there is realy no ship left in the fleet!
                        remove_ship(&fl->region->ships, fl);
                    }
                    break;
                        }
                    }

                    sh->fleet = u->ship;
                    sh->_owner = ship_owner(u->ship);

                }
            }
            if (*ordp == ord)
                ordp = &ord->next;
        }
        if (*uptr == u)
            uptr = &u->next;
    }
}
