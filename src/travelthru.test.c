﻿#include <platform.h>
#include <kernel/config.h>
#include <kernel/region.h>
#include <kernel/unit.h>
#include <kernel/faction.h>
#include <util/attrib.h>

#include "travelthru.h"
#include "reports.h"
#include "tests.h"

#include <CuTest.h>

struct attrib;

static void count_travelers(region *r, unit *u, void *cbdata) {
    int *n = (int *)cbdata;
    unused_arg(r);
    *n += u->number;
}

typedef struct travel_fixture {
    region *r;
    faction *f;
} travel_fixture;

static void setup_travelthru(travel_fixture *fix, int nunits) {
    region *r;
    faction *f;

    test_cleanup();
    r = test_create_region(0, 0, 0);
    while (r->attribs) {
        a_remove(&r->attribs, r->attribs);
    }
    f = test_create_faction(0);
    while (nunits--) {
        unit *u = test_create_unit(f, 0);
        travelthru_add(r, u);
    }
    fix->r = r;
    fix->f = f;
}

static void test_travelthru_count(CuTest *tc) {
    travel_fixture fix;
    setup_travelthru(&fix, 0);
    CuAssertIntEquals(tc, 0, count_travelthru(fix.r, fix.f));

    setup_travelthru(&fix, 1);
    CuAssertIntEquals(tc, 1, count_travelthru(fix.r, fix.f));

    setup_travelthru(&fix, 2);
    CuAssertIntEquals(tc, 2, count_travelthru(fix.r, fix.f));

    test_cleanup();
}

static void test_travelthru_map(CuTest *tc) {
    int n = 0;
    travel_fixture fix;

    setup_travelthru(&fix, 0);
    travelthru_map(fix.r, count_travelers, &n);
    CuAssertIntEquals(tc, 0, n);

    setup_travelthru(&fix, 1);
    travelthru_map(fix.r, count_travelers, &n);
    CuAssertIntEquals(tc, 1, n);

    test_cleanup();
}

CuSuite *get_travelthru_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_travelthru_count);
    SUITE_ADD_TEST(suite, test_travelthru_map);
    return suite;
}
