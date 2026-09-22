CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -O0 -g -I../main -I../core

# api_scan depends on cJSON, which ships with ESP-IDF. We compile it
# in via $(IDF_PATH); no toolchain is needed, only the source files.
# Relax warnings on the third-party cJSON.c only.
IDF_PATH ?= $(HOME)/tools/esp/esp-idf
CJSON_DIR := $(IDF_PATH)/components/json/cJSON

BINS = test_strbuf test_sip_parse test_sip_frame test_sip_srv test_sip_auth test_sip_response test_tr064_parse test_phone_norm test_api_scan test_log_capture test_improv_proto test_blocklist_lookup test_sched_time test_smtp_body test_mail_html test_mail_rcpt test_name_filter test_version_cmp test_net_local test_heap_guard test_platform_linux test_config_file test_config_linux test_sip_transport_linux test_http_client_linux test_phoneblock_api_linux test_tr064_client_linux test_sip_register_linux test_rtp_linux

test_sip_parse: test_sip_parse.c ../core/sip_parse.c ../core/sip_parse.h \
				../core/dial_rules.c ../core/dial_rules.h \
                ../main/audio.c ../main/audio.h
	$(CC) $(CFLAGS) test_sip_parse.c ../core/sip_parse.c ../core/dial_rules.c ../main/audio.c -o $@

test_sip_frame: test_sip_frame.c ../main/sip_frame.c ../main/sip_frame.h \
                ../core/sip_parse.c ../core/sip_parse.h ../core/dial_rules.c
	$(CC) $(CFLAGS) test_sip_frame.c ../main/sip_frame.c ../core/sip_parse.c ../core/dial_rules.c -o $@

test_sip_srv: test_sip_srv.c ../main/sip_srv.c ../main/sip_srv.h
	$(CC) $(CFLAGS) test_sip_srv.c ../main/sip_srv.c -o $@

test_sip_auth: test_sip_auth.c ../main/sip_auth.c ../main/sip_auth.h
	$(CC) $(CFLAGS) test_sip_auth.c ../main/sip_auth.c -o $@

test_tr064_parse: test_tr064_parse.c ../core/tr064_parse.c ../core/tr064_parse.h
	$(CC) $(CFLAGS) -I../core test_tr064_parse.c ../core/tr064_parse.c -o $@

test_phone_norm: test_phone_norm.c ../main/phone_norm.c ../main/phone_norm.h \
                 ../core/dial_rules.c ../core/dial_rules.h
	$(CC) $(CFLAGS) test_phone_norm.c ../main/phone_norm.c ../core/dial_rules.c -o $@

test_log_capture: test_log_capture.c ../main/log_capture.c ../main/log_capture.h
	$(CC) $(CFLAGS) test_log_capture.c ../main/log_capture.c -o $@

test_improv_proto: test_improv_proto.c ../main/improv_proto.c ../main/improv_proto.h
	$(CC) $(CFLAGS) test_improv_proto.c ../main/improv_proto.c -o $@

cjson.o: $(CJSON_DIR)/cJSON.c $(CJSON_DIR)/cJSON.h
	$(CC) -O0 -g -I$(CJSON_DIR) -c $< -o $@

test_api_scan: test_api_scan.c ../main/api_scan.c ../main/api_scan.h cjson.o
	$(CC) $(CFLAGS) -I$(CJSON_DIR) test_api_scan.c ../main/api_scan.c cjson.o -o $@

test_blocklist_lookup: test_blocklist_lookup.c ../main/blocklist_lookup.c ../main/blocklist_lookup.h
	$(CC) $(CFLAGS) test_blocklist_lookup.c ../main/blocklist_lookup.c -o $@

test_sched_time: test_sched_time.c ../main/sched_time.c ../main/sched_time.h
	$(CC) $(CFLAGS) test_sched_time.c ../main/sched_time.c -o $@

test_smtp_body: test_smtp_body.c ../main/smtp_body.c ../main/smtp_body.h
	$(CC) $(CFLAGS) test_smtp_body.c ../main/smtp_body.c -o $@

test_mail_html: test_mail_html.c ../main/mail_html.c ../main/mail_html.h
	$(CC) $(CFLAGS) test_mail_html.c ../main/mail_html.c -o $@

test_mail_rcpt: test_mail_rcpt.c ../main/mail_rcpt.c ../main/mail_rcpt.h ../main/strbuf.h
	$(CC) $(CFLAGS) test_mail_rcpt.c ../main/mail_rcpt.c -o $@

test_name_filter: test_name_filter.c ../main/name_filter.c ../main/name_filter.h
	$(CC) $(CFLAGS) test_name_filter.c ../main/name_filter.c -o $@

test_version_cmp: test_version_cmp.c ../main/version_cmp.c ../main/version_cmp.h
	$(CC) $(CFLAGS) test_version_cmp.c ../main/version_cmp.c -o $@

test_net_local: test_net_local.c ../main/net_local.c ../main/net_local.h
	$(CC) $(CFLAGS) test_net_local.c ../main/net_local.c -o $@

test_heap_guard: test_heap_guard.c ../main/heap_guard_scan.c ../main/heap_guard_scan.h
	$(CC) $(CFLAGS) test_heap_guard.c ../main/heap_guard_scan.c -o $@

test_platform_linux: test_platform_linux.c ../core/platform_linux.c ../core/platform.h
	$(CC) $(CFLAGS) -pthread test_platform_linux.c ../core/platform_linux.c -o $@

test_config_file: test_config_file.c ../frontend_linux/config_file.c ../frontend_linux/config_file.h
	$(CC) $(CFLAGS) -I../frontend_linux test_config_file.c ../frontend_linux/config_file.c -o $@

test_config_linux: test_config_linux.c ../frontend_linux/config_linux.c ../frontend_linux/config_linux.h \
                   ../frontend_linux/config_file.c ../frontend_linux/config_file.h
	$(CC) $(CFLAGS) -I../frontend_linux test_config_linux.c ../frontend_linux/config_linux.c \
        ../frontend_linux/config_file.c -o $@

test_sip_transport_linux: test_sip_transport_linux.c ../frontend_linux/sip_transport_linux.c \
			  ../main/sip_transport.h ../core/platform_linux.c ../core/platform.h
	$(CC) $(CFLAGS) -I../frontend_linux -pthread test_sip_transport_linux.c \
	../frontend_linux/sip_transport_linux.c ../core/platform_linux.c -o $@

test_http_client_linux: test_http_client_linux.c ../frontend_linux/http_client_linux.c \
			../frontend_linux/http_client_linux.h
	$(CC) $(CFLAGS) -I../frontend_linux test_http_client_linux.c \
	../frontend_linux/http_client_linux.c $$(pkg-config --cflags --libs libcurl) -o $@

test_phoneblock_api_linux: test_phoneblock_api_linux.c ../frontend_linux/phoneblock_api_linux.c \
			   ../frontend_linux/phoneblock_api_linux.h ../main/api_scan.c \
			   ../main/api_scan.h ../frontend_linux/http_client_linux.c \
			   ../frontend_linux/http_client_linux.h
	$(CC) $(CFLAGS) -I../frontend_linux -I$(CJSON_DIR) test_phoneblock_api_linux.c \
	../frontend_linux/phoneblock_api_linux.c ../main/api_scan.c \
	../frontend_linux/http_client_linux.c $(CJSON_DIR)/cJSON.c \
	$$(pkg-config --cflags --libs libcurl openssl) -o $@

test_tr064_client_linux: test_tr064_client_linux.c ../frontend_linux/tr064_client_linux.c \
			 ../frontend_linux/tr064_client_linux.h ../frontend_linux/http_client_linux.c
	$(CC) $(CFLAGS) -I../frontend_linux -pthread test_tr064_client_linux.c \
	../frontend_linux/tr064_client_linux.c ../frontend_linux/http_client_linux.c \
	$$(pkg-config --cflags --libs libcurl) -o $@

test_sip_register_linux: test_sip_register_linux.c ../frontend_linux/sip_register_linux.c \
			 ../frontend_linux/sip_register_linux.h ../main/sip_auth.c \
			 ../core/sip_parse.c ../core/sip_parse.h ../core/dial_rules.c \
			 ../main/audio.c ../frontend_linux/sip_transport_linux.c
	$(CC) $(CFLAGS) -I../frontend_linux -pthread test_sip_register_linux.c \
	../frontend_linux/sip_register_linux.c ../main/sip_auth.c \
	../core/sip_parse.c ../core/dial_rules.c ../main/audio.c \
	../frontend_linux/sip_transport_linux.c ../core/platform_linux.c \
	$$(pkg-config --cflags --libs openssl) -o $@

test_rtp_linux: test_rtp_linux.c ../frontend_linux/rtp_linux.c ../frontend_linux/rtp_linux.h \
		../core/platform_linux.c ../core/platform.h
	$(CC) $(CFLAGS) -I../frontend_linux -pthread test_rtp_linux.c ../frontend_linux/rtp_linux.c \
	../core/platform_linux.c -o $@

phoneblock_linux: ../frontend_linux/main.c ../frontend_linux/config_linux.c \
		  ../frontend_linux/config_linux.h ../frontend_linux/config_file.c \
		  ../frontend_linux/config_file.h ../frontend_linux/phoneblock_api_linux.c \
		  ../frontend_linux/phoneblock_api_linux.h ../frontend_linux/http_client_linux.c \
				  ../frontend_linux/sip_transport_linux.c ../main/sip_transport.h \
				  ../frontend_linux/sip_register_linux.c ../frontend_linux/sip_register_linux.h \
				  ../frontend_linux/sip_server_linux.c ../frontend_linux/sip_server_linux.h \
				  ../frontend_linux/phoneblock_api_linux.c \
				  ../main/sip_response.c ../main/sip_response.h \
				  ../main/sip_auth.c ../core/sip_parse.c ../core/dial_rules.c ../main/audio.c \
				  ../main/api_scan.c ../main/api_scan.h ../core/platform_linux.c ../core/platform.h
	$(CC) $(CFLAGS) -I../frontend_linux -pthread ../frontend_linux/main.c \
	../frontend_linux/config_linux.c ../frontend_linux/config_file.c \
	../frontend_linux/phoneblock_api_linux.c ../frontend_linux/http_client_linux.c \
	../frontend_linux/sip_transport_linux.c ../main/api_scan.c \
	../frontend_linux/sip_register_linux.c ../main/sip_auth.c \
	../core/sip_parse.c ../core/dial_rules.c ../main/audio.c \
	../frontend_linux/sip_server_linux.c ../frontend_linux/phoneblock_api_linux.c \
	../main/sip_response.c \
	$(CJSON_DIR)/cJSON.c ../core/platform_linux.c \
	-I$(CJSON_DIR) $$(pkg-config --cflags --libs libcurl openssl) -o $@

# strbuf.h is header-only; build its test with AddressSanitizer so any overrun
# of the (malloc'd) destination buffer aborts.
test_strbuf: test_strbuf.c ../main/strbuf.h
	$(CC) $(CFLAGS) -fsanitize=address test_strbuf.c -o $@

# Built with AddressSanitizer: this test exists to catch buffer overruns in
# the SIP response builder, so the sanitizer must be active for it to mean
# anything. sip_response.c / the test define _GNU_SOURCE themselves (memmem).
test_sip_response: test_sip_response.c ../main/sip_response.c ../main/sip_response.h \
				   ../core/sip_parse.c ../core/sip_parse.h ../main/audio.c \
				   ../core/dial_rules.c
	$(CC) $(CFLAGS) -fsanitize=address test_sip_response.c ../main/sip_response.c ../core/sip_parse.c ../core/dial_rules.c ../main/audio.c -o $@

test: $(BINS)
	@set -e; for b in $(BINS); do echo "→ $$b"; ./$$b; done
	@echo "→ test-signing-roundtrip.sh"; ./test-signing-roundtrip.sh

clean:
	rm -f $(BINS) cjson.o

.PHONY: test clean
