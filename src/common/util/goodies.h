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

#ifndef GOODIES_H
#define GOODIES_H
#ifdef __cplusplus
extern "C" {
#endif

extern char * set_string(char **s, const char *neu);
extern int set_email(char** pemail, const char *newmail);

extern int *intlist_init(void);
extern int *intlist_add(int *i_p, int i);
extern int *intlist_find(int *i_p, int i);

#ifdef HAVE_INLINE
# include "strings.c"
#else
extern unsigned int hashstring(const char* s);
extern const char *escape_string(const char * str, char * buffer, unsigned int len);
extern unsigned int jenkins_hash(unsigned int a);
extern unsigned int wang_hash(unsigned int a);
#endif

/* benchmark for units: 
 * JENKINS_HASH: 5.25 misses/hit (with good cache behavior)
 * WANG_HASH:    5.33 misses/hit (with good cache behavior)
 * KNUTH_HASH:   1.93 misses/hit (with bad cache behavior)
 * CF_HASH:      fucking awful!
 */
#define KNUTH_HASH1(a, m) ((a) % m)
#define KNUTH_HASH2(a, m) (m - 2 - a % (m-2))
#define CF_HASH1(a, m) ((a) % m)
#define CF_HASH2(a, m) (8 - ((a) & 7))
#define JENKINS_HASH1(a, m) (jenkins_hash(a) % m)
#define JENKINS_HASH2(a, m) 1
#define WANG_HASH1(a, m) (wang_hash(a) % m)
#define WANG_HASH2(a, m) 1

#define HASH1 JENKINS_HASH1
#define HASH2 JENKINS_HASH2

#define SWAP_VARS(T, a, b) { T x = a; a = b; b = x; }
#ifdef __cplusplus
}
#endif
#endif /* GOODIES_H */
