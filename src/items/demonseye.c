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
#include "demonseye.h"

/* kernel includes */
#include <kernel/faction.h>
#include <kernel/item.h>
#include <kernel/messages.h>
#include <kernel/plane.h>
#include <kernel/region.h>
#include <kernel/unit.h>

/* util includes */
#include <util/functions.h>

/* libc includes */
#include <assert.h>

static int
summon_igjarjuk(struct unit *u, const struct item_type *itype, int amount,
struct order *ord)
{
    struct plane *p = rplane(u->region);
    unused_arg(amount);
    unused_arg(itype);
    if (p != NULL) {
        ADDMSG(&u->faction->msgs, msg_feedback(u, ord, "use_realworld_only", ""));
        return EUNUSABLE;
    }
    else {
        assert(!"not implemented");
        return EUNUSABLE;
    }
}

static int
give_igjarjuk(struct unit *src, struct unit *d, const struct item_type *itype,
int n, struct order *ord)
{
    ADDMSG(&src->faction->msgs, msg_feedback(src, ord, "error_giveeye", ""));
    return 0;
}

void register_demonseye(void)
{
    register_item_use(summon_igjarjuk, "useigjarjuk");
    register_item_give(give_igjarjuk, "giveigjarjuk");
}
