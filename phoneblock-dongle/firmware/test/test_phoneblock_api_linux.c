#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "phoneblock_api_linux.h"

int main(void)
{
    const char json[] =
        "{\"numbers\":[{\"phone\":\"+49301234567\",\"votes\":5,"
        "\"label\":\"Test spam\",\"location\":\"Berlin\"}],"
        "\"range10\":[],\"range100\":[]}";
    pb_check_result_t result;
    assert(pb_linux_classify_check("+49301234567", json, strlen(json),
                                   4, 10, &result) == 0);
    assert(result.verdict == VERDICT_SPAM);
    assert(result.assessment == PB_ASSESS_SPAM);
    assert(result.direct_votes == 5);
    assert(strcmp(result.label, "Test spam") == 0);
    assert(strcmp(result.location, "Berlin") == 0);
    puts("test_phoneblock_api_linux: all tests passed");
    return 0;
}