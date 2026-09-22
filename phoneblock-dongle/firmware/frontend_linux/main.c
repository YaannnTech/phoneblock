#include "config_linux.h"
#include "phoneblock_api_linux.h"
#include "platform.h"
#include "sip_register_linux.h"
#include "sip_server_linux.h"
#include "sip_transport.h"

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
    static char path[512];
    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home && config_home[0]) {
        snprintf(path, sizeof(path), "%s/phoneblock/dongle.conf", config_home);
    } else {
        snprintf(path, sizeof(path), "/etc/phoneblock/dongle.conf");
    }
    return path;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--config PATH] [--foreground] [--check-config] "
                    "[--check-number NUMBER] [--probe-sip] [--register-sip] "
                    "[--service] [--listen-sip]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *config_path = default_config_path();
    const char *check_number = NULL;
    int probe_sip = 0;
    int register_sip = 0;
    int service_mode = 0;
    int listen_sip = 0;
    int check_config = 0;

    static const struct option options[] = {
        { "config", required_argument, NULL, 'c' },
        { "foreground", no_argument, NULL, 'f' },
        { "check-config", no_argument, NULL, 't' },
        { "check-number", required_argument, NULL, 'n' },
        { "probe-sip", no_argument, NULL, 'p' },
        { "register-sip", no_argument, NULL, 'r' },
        { "service", no_argument, NULL, 's' },
        { "listen-sip", no_argument, NULL, 'l' },
        { NULL, 0, NULL, 0 }
    };
    int option;
    while ((option = getopt_long(argc, argv, "c:ftn:prsl", options, NULL)) != -1) {
        switch (option) {
            case 'c': config_path = optarg; break;
            case 'f': break;
            case 't': check_config = 1; break;
            case 'n': check_number = optarg; break;
            case 'p': probe_sip = 1; break;
            case 'r': register_sip = 1; break;
            case 's': service_mode = 1; break;
            case 'l': listen_sip = 1; break;
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

    if (check_number) {
        pb_check_result_t result;
        int check_result = pb_linux_phoneblock_check(
            config.phoneblock_base_url, config.phoneblock_token,
            check_number, 4, 10, &result);
        if (check_result != 0) {
            pb_log_err("linux", "PhoneBlock check failed for %s", check_number);
            return EXIT_FAILURE;
        }
        pb_log_info("linux", "PhoneBlock result for %s: %s",
                    check_number,
                    result.verdict == VERDICT_SPAM ? "SPAM" : "LEGITIMATE");
        return EXIT_SUCCESS;
    }
    if (probe_sip) {
        sip_transport_t *transport = sip_transport_open(
            config.sip_host[0] ? "udp" : "", config.sip_host,
            config.sip_port, NULL, config.sip_local_port);
        if (!transport) {
            pb_log_err("linux", "SIP transport probe failed");
            return EXIT_FAILURE;
        }
        pb_log_info("linux", "SIP transport ready at %s:%d",
                    sip_transport_local_ip(transport),
                    sip_transport_local_port(transport));
        sip_transport_close(transport);
        return EXIT_SUCCESS;
    }
    if (register_sip) {
        int status;
        char challenge[256];
        int result = pb_linux_sip_register_probe(
            config.sip_host, config.sip_port, config.sip_user,
            config.sip_pass, config.sip_local_port, &status,
            challenge, sizeof(challenge));
        if (result != 0) {
            pb_log_err("linux", "SIP REGISTER exchange failed");
            return EXIT_FAILURE;
        }
        pb_log_info("linux", "SIP REGISTER completed with status %d", status);
        return status == 200 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (check_config) return EXIT_SUCCESS;

    struct sigaction action = { .sa_handler = handle_signal };
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    pb_log_info("linux", "service skeleton running");
    if (listen_sip) {
        int result = pb_linux_sip_listen(
            config.sip_host, config.sip_port, config.sip_user,
            config.sip_local_port, config.phoneblock_base_url,
            config.phoneblock_token, config.announcement_path,
            config.rtp_port,
            &shutdown_requested);
        if (result != 0) return EXIT_FAILURE;
        pb_log_info("linux", "SIP listener stopped");
        return EXIT_SUCCESS;
    }
    if (service_mode) {
        while (!shutdown_requested) {
            int status = 0;
            char challenge[256];
            int result = pb_linux_sip_register_probe(
                config.sip_host, config.sip_port, config.sip_user,
                config.sip_pass, config.sip_local_port, &status,
                challenge, sizeof(challenge));
            if (result == 0 && status == 200) {
                pb_log_info("linux", "SIP service registered; refreshing in 1800 s");
                for (int second = 0; second < 1800 && !shutdown_requested; second++) {
                    pb_task_sleep_ms(1000);
                }
            } else {
                pb_log_warn("linux", "SIP registration failed (status %d); retrying in 30 s",
                            status);
                for (int second = 0; second < 30 && !shutdown_requested; second++) {
                    pb_task_sleep_ms(1000);
                }
            }
        }
    } else {
        while (!shutdown_requested) pause();
    }
    pb_log_info("linux", "shutdown requested");
    return EXIT_SUCCESS;
}