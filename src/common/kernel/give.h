/* vi: set ts=2:
 +-------------------+  Christian Schlittchen <corwin@amber.kn-bremen.de>
 |                   |  Enno Rehling <enno@eressea-pbem.de>
 | Eressea PBEM host |  Katja Zedel <katze@felidae.kn-bremen.de>
 | (c) 1998 - 2003   |  Henning Peters <faroul@beyond.kn-bremen.de>
 |                   |  Ingo Wilken <Ingo.Wilken@informatik.uni-oldenburg.de>
 +-------------------+  Stefan Reich <reich@halbling.de>

 This program may not be used, modified or distributed 
 without prior permission by the authors of Eressea.

 */
#ifndef H_KRNL_GIVE
#define H_KRNL_GIVE
#ifdef __cplusplus
extern "C" {
#endif

  extern int give_item(int want, const struct item_type * itype, struct unit * src, struct unit * dest, struct order * ord);
  extern void givemen(int n, struct unit * u, struct unit * u2, struct order * ord);
  extern void giveunit(struct unit * u, struct unit * u2, struct order * ord);

#ifdef __cplusplus
}
#endif
#endif
