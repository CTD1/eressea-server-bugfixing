/* vi: set ts=2:
 *
 *	
 *	Eressea PB(E)M host Copyright (C) 1998
 *		Enno Rehling (rehling@usa.net)
 *		Christian Schlittchen (corwin@amber.kn-bremen.de)
 *		Katja Zedel (katze@felidae.kn-bremen.de)
 *		Henning Peters (faroul@beyond.kn-bremen.de)
 *
 *  based on:
 *
 * Atlantis v1.0  13 September 1993 Copyright 1993 by Russell Wallace
 * Atlantis v1.7                    Copyright 1996 by Alex Schr�der
 *
 * This program may not be used, modified or distributed. It may not be
 * sold or used commercially without prior written permission from the
 * authors.
 */

#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

/** rolls a number of n-sided dice.
 * Usage: 3d6-3d4+5 = dice(3,6)-dice(3,4)+5 */
int
dice(int count, int value)
{
  int d = 0, c;

  if (value<=0) return 0; /* (enno) %0 geht nicht. echt wahr. */
  if (count >= 0)
    for (c = count; c > 0; c--)
      d += rand() % value + 1;
  else
    for (c = count; c < 0; c++)
      d -= rand() % value + 1;

  return d;
}

/** Parses a string of the form "2d6+8"
* Kann nur simple Strings der Form "xdy[+-]z" parsen!
* Sch�ner w�re eine flexibele Routine, die z.B. auch Sachen wie 2d6+3d4-1
* parsen kann. */
int
dice_rand(const char *s)
{
  const char *c = s;
  int m = 0, d = 0, k = 0, multi = 1;
  int state = 1;

  for (;;) {
    if (isdigit((int)*c)) {
      k = k*10+(*c-'0');
    } else if (*c=='+' || *c=='-' || *c==0) {
      if (state==1) /* konstante k addieren */
        m+=k*multi;
      else if (state==2) { /* dDk */
        int i;
        if (k == 0) k = 6; /* 3d == 3d6 */
        for (i=0;i!=d;++i) m += (1 + rand() % k)*multi;
      }
      else assert(!"dice_rand: illegal token");
      k = d = 0;
      state = 1;
      multi = (*c=='-')?-1:1;
    } else if (*c=='d' || *c=='D') {
      if (k==0) k = 1; /* d9 == 1d9 */
      assert(state==1 || !"dice_rand: illegal token");
      d = k;
      k = 0;
      state=2;
    } else assert(!"dice_rand: illegal token");
    if (*c==0) break;
    c++;
  }
  return m;
}
