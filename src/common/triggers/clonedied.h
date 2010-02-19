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

#ifndef CLONEDIED_H
#define CLONEDIED_H
#ifdef __cplusplus
extern "C" {
#endif

struct trigger_type;
struct trigger;

struct unit;

extern struct trigger_type tt_clonedied;
extern struct trigger * trigger_clonedied(struct unit * u);

#ifdef __cplusplus
}
#endif
#endif
