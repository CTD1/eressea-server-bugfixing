#include "prefix.h"

#include <tests.h>

#include <stddef.h>
#include <CuTest.h>

static void test_add_prefix(CuTest *tc) {
    test_cleanup();
    CuAssertPtrEquals(tc, 0, race_prefixes);
    add_raceprefix("sea");
    CuAssertPtrNotNull(tc, race_prefixes);
    CuAssertStrEquals(tc, "sea", race_prefixes[0]);
    CuAssertPtrEquals(tc, 0, race_prefixes[1]);
    add_raceprefix("moon");
    CuAssertStrEquals(tc, "sea", race_prefixes[0]);
    CuAssertStrEquals(tc, "moon", race_prefixes[1]);
    CuAssertPtrEquals(tc, 0, race_prefixes[2]);
    free_prefixes();
    CuAssertPtrEquals(tc, 0, race_prefixes);
    test_cleanup();
}

CuSuite *get_prefix_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_add_prefix);
    return suite;
}