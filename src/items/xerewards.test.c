#include <platform.h>

#include "xerewards.h"

#include <kernel/unit.h>
#include <kernel/item.h>
#include <kernel/pool.h>
#include <kernel/region.h>

#include <tests.h>
#include <CuTest.h>

static void test_manacrystal(CuTest *tc) {
    test_cleanup();
    test_create_world();
    test_cleanup();
}

static void test_skillpotion(CuTest *tc) {
    unit *u;
    const struct item_type *itype;

    test_cleanup();
    test_create_world();
    u = test_create_unit(test_create_faction(NULL), findregion(0, 0));
    itype = test_create_itemtype("skillpotion");
    change_resource(u, itype->rtype, 2);
    CuAssertIntEquals(tc, 1, use_skillpotion(u, itype, 1, NULL));
    test_cleanup();
}

CuSuite *get_xerewards_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_manacrystal);
    SUITE_ADD_TEST(suite, test_skillpotion);
    return suite;
}

