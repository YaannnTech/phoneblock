#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config_linux.h"

int main(void)
{
    const char *path = "test_config_linux.tmp";
    remove(path);

    pb_linux_config_t config;
    assert(pb_linux_config_load(path, &config) == 0);
    assert(config.sip_port == 5060);
    assert(config.sip_local_port == 15060);
    assert(config.rtp_port == 16000);

    snprintf(config.sip_host, sizeof(config.sip_host), "fritz.box");
    snprintf(config.sip_user, sizeof(config.sip_user), "620");
    snprintf(config.sip_pass, sizeof(config.sip_pass), "secret");
    config.sip_expires = 1800;
    assert(pb_linux_config_save(path, &config) == 0);

    pb_linux_config_t reloaded;
    assert(pb_linux_config_load(path, &reloaded) == 0);
    assert(strcmp(reloaded.sip_host, "fritz.box") == 0);
    assert(strcmp(reloaded.sip_user, "620") == 0);
    assert(strcmp(reloaded.sip_pass, "secret") == 0);
    assert(reloaded.sip_expires == 1800);
    assert(reloaded.rtp_port == 16000);

    remove(path);
    puts("test_config_linux: all tests passed");
    return 0;
}