#pragma once
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

#ifndef H_SEEN_REGION
#define H_SEEN_REGION

struct region;
struct faction;
struct seen_region;

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        see_none,
        see_neighbour,
        see_lighthouse,
        see_travel,
        see_far,
        see_unit,
        see_battle
    } seen_t;

    typedef struct seen_region {
        struct seen_region *nextHash;
        struct seen_region *next;
        struct region *r;
        seen_t mode;
        bool disbelieves;
    } seen_region;

struct seen_region **seen_init(void);
void seen_done(struct seen_region *seehash[]);
void free_seen(void);
void link_seen(struct seen_region *seehash[], const struct region * first, const struct region * last);
struct seen_region *find_seen(struct seen_region *seehash[], const struct region * r);
void get_seen_interval(struct seen_region *seen[], struct region **firstp, struct region **lastp);
seen_region *add_seen(struct seen_region *seehash[], struct region *r, seen_t mode, bool dis);
void link_seen(struct seen_region *seehash[], const struct region *first, const struct region *last);
void seenhash_map(struct seen_region *seen[], void(*cb)(struct seen_region *, void *), void *cbdata);
struct seen_region *faction_add_seen(struct faction *f, struct region *r, seen_t mode);
#ifdef __cplusplus
}
#endif
#endif
