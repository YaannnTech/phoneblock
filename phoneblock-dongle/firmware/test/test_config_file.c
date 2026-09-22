#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config_file.h"

int main(void)
{
    const char *path = "test_config_file.tmp";
    FILE *output = fopen(path, "w");
    assert(output != NULL);
    fputs("# comment\n"
          " sip_host = fritz.box \n"
          "token=abc=def\n"
          "sip_host=router.local\n"
          "; ignored\n", output);
    fclose(output);

    pb_config_file_t config;
    assert(pb_config_file_load(path, &config) == 0);
    char value[PB_CONFIG_VALUE_CAP];
    assert(pb_config_file_get(&config, "sip_host", value, sizeof(value)) == 1);
    assert(strcmp(value, "router.local") == 0);
    assert(pb_config_file_get(&config, "token", value, sizeof(value)) == 1);
    assert(strcmp(value, "abc=def") == 0);
    assert(pb_config_file_get(&config, "missing", value, sizeof(value)) == 0);

    assert(pb_config_file_set(&config, "sip_port", "5060") == 0);
    assert(pb_config_file_set(&config, "sip_host", "fritzbox.local") == 0);
    assert(pb_config_file_save(path, &config) == 0);
    pb_config_file_t reloaded;
    assert(pb_config_file_load(path, &reloaded) == 0);
    assert(pb_config_file_get(&reloaded, "sip_host", value, sizeof(value)) == 1);
    assert(strcmp(value, "fritzbox.local") == 0);
    assert(pb_config_file_get(&reloaded, "sip_port", value, sizeof(value)) == 1);
    assert(strcmp(value, "5060") == 0);
    assert(pb_config_file_get_int(&reloaded, "sip_port", 15060, 1, 65535) == 5060);
    assert(pb_config_file_get_int(&reloaded, "missing", 15060, 1, 65535) == 15060);
    assert(pb_config_file_set(&reloaded, "enabled", "yes") == 0);
    assert(pb_config_file_get_bool(&reloaded, "enabled", 0) == 1);
    assert(pb_config_file_set(&reloaded, "enabled", "maybe") == 0);
    assert(pb_config_file_get_bool(&reloaded, "enabled", 1) == 1);

    remove(path);
    puts("test_config_file: all tests passed");
    return 0;
}