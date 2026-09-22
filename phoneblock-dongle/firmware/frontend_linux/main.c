#include "config_linux.h"
#include "platform.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t shutdown_requested;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    shutdown_requested = 1;
}

static const char *default_config_path(void)
{
    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home && config_home[0]) return config_home;
    return "/etc/phoneblock";
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--config PATH] [--foreground] [--check-config]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *config_path = default_config_path();
    int check_config = 0;

    static const struct option options[] = {
        { "config", required_argument, NULL, 'c' },
        { "foreground", no_argument, NULL, 'f' },
        { "check-config", no_argument, NULL, 't' },
        { NULL, 0, NULL, 0 }
    };
    int option;
    while ((option = getopt_long(argc, argv, "c:ft", options, NULL)) != -1) {
        switch (option) {
            case 'c': config_path = optarg; break;
            case 'f': break;
            case 't': check_config = 1; break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    pb_linux_config_t config;
    if (pb_linux_config_load(config_path, &config) != 0) {
        pb_log_err("linux", "cannot load configuration from %s", config_path);
        return EXIT_FAILURE;
    }
    pb_log_info("linux", "configuration loaded from %s", config_path);
    pb_log_info("linux", "SIP registrar %s:%d, local SIP port %d, RTP port %d",
                config.sip_host[0] ? config.sip_host : "<unset>",
                config.sip_port, config.sip_local_port, config.rtp_port);

    if (check_config) return EXIT_SUCCESS;

    struct sigaction action = { .sa_handler = handle_signal };
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    pb_log_info("linux", "service skeleton running");
    while (!shutdown_requested) pause();
    pb_log_info("linux", "shutdown requested");
    return EXIT_SUCCESS;
}