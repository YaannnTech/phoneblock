#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "http_client_linux.h"

int main(void)
{
    const char *path = "/tmp/pb-http-client-test.txt";
    FILE *output = fopen(path, "w");
    assert(output != NULL);
    fputs("PhoneBlock test response", output);
    fclose(output);

    char url[128];
    snprintf(url, sizeof(url), "file://%s", path);
    pb_http_response_t response;
    assert(pb_http_get(url, NULL, 1024, &response) == 0);
    assert(response.status == 0);
    assert(strcmp(response.body, "PhoneBlock test response") == 0);
    pb_http_response_free(&response);
    remove(path);
    puts("test_http_client_linux: all tests passed");
    return 0;
}