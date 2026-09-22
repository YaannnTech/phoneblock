#include "phoneblock_api_linux.h"

#include "api_scan.h"
#include "http_client_linux.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>

#define HASH_PREFIX_HEX 4
#define MIN_AGGREGATE_10 4
#define MIN_AGGREGATE_100 3

static void sha1_hex_prefix(const char *input, char *output)
{
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)input, strlen(input), digest);
    static const char hex[] = "0123456789ABCDEF";
    for (int index = 0; index < HASH_PREFIX_HEX / 2; index++) {
        output[index * 2] = hex[(digest[index] >> 4) & 0xf];
        output[index * 2 + 1] = hex[digest[index] & 0xf];
    }
    output[HASH_PREFIX_HEX] = '\0';
}

static int wildcard_votes(int votes10, int count10,
                           int votes100, int count100)
{
    if (count100 >= MIN_AGGREGATE_100) {
        return votes100 + (count10 < MIN_AGGREGATE_10 ? votes10 : 0);
    }
    return count10 >= MIN_AGGREGATE_10 ? votes10 : 0;
}

int pb_linux_classify_check(const char *phone_number, const char *json,
                            size_t json_length, int minimum_direct,
                            int minimum_range, pb_check_result_t *out)
{
    if (!phone_number || !json || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->verdict = VERDICT_ERROR;
    out->assessment = PB_ASSESS_ERROR;

    api_scan_t scan;
    api_scan_init(&scan, phone_number);
    api_scan_feed(&scan, json, (int)json_length);
    if (scan.error) return -1;

    int range = wildcard_votes(scan.v10, scan.c10, scan.v100, scan.c100);
    bool direct_hit = scan.direct_votes >= minimum_direct;
    bool range_hit = minimum_range >= 1 && range >= minimum_range;
    verdict_t verdict;
    if (scan.white_listed) {
        verdict = VERDICT_LEGITIMATE;
        out->assessment = PB_ASSESS_LEGITIMATE;
    } else if (scan.black_listed) {
        verdict = VERDICT_SPAM;
        out->assessment = PB_ASSESS_BLACKLIST;
    } else {
        verdict = (direct_hit || range_hit) ? VERDICT_SPAM : VERDICT_LEGITIMATE;
        out->assessment = verdict == VERDICT_SPAM
            ? PB_ASSESS_SPAM : PB_ASSESS_UNKNOWN;
    }
    out->verdict = verdict;
    out->direct_votes = scan.direct_votes;
    out->range_votes = scan.v10 > scan.v100 ? scan.v10 : scan.v100;
    memcpy(out->label, scan.label, sizeof(out->label));
    memcpy(out->location, scan.location, sizeof(out->location));
    return 0;
}

int pb_linux_phoneblock_check(const char *base_url, const char *token,
                              const char *phone_number, int minimum_direct,
                              int minimum_range, pb_check_result_t *out)
{
    if (!base_url || !phone_number || !out) return -1;
    char full_hash[HASH_PREFIX_HEX + 1];
    char prefix10[HASH_PREFIX_HEX + 1];
    char prefix100[HASH_PREFIX_HEX + 1];
    char shortened[64];
    sha1_hex_prefix(phone_number, full_hash);
    int phone_length = (int)strlen(phone_number);
    int url_length = snprintf(NULL, 0,
        "%s/api/check-prefix?sha1=%s&format=json", base_url, full_hash);
    if (phone_length > 1) {
        memcpy(shortened, phone_number, (size_t)phone_length - 1);
        shortened[phone_length - 1] = '\0';
        sha1_hex_prefix(shortened, prefix10);
        url_length += snprintf(NULL, 0, "&prefix10=%s", prefix10);
    }
    if (phone_length > 2) {
        memcpy(shortened, phone_number, (size_t)phone_length - 2);
        shortened[phone_length - 2] = '\0';
        sha1_hex_prefix(shortened, prefix100);
        url_length += snprintf(NULL, 0, "&prefix100=%s", prefix100);
    }
    char url[(size_t)url_length + 1];
    snprintf(url, sizeof(url), "%s/api/check-prefix?sha1=%s&format=json",
             base_url, full_hash);
    if (phone_length > 1) {
        snprintf(url + strlen(url), sizeof(url) - strlen(url),
                 "&prefix10=%s", prefix10);
    }
    if (phone_length > 2) {
        snprintf(url + strlen(url), sizeof(url) - strlen(url),
                 "&prefix100=%s", prefix100);
    }

    pb_http_response_t response;
    if (pb_http_get(url, token, 1024 * 1024, &response) != 0) return -1;
    int result = response.status == 200
        ? pb_linux_classify_check(phone_number, response.body,
                                  response.length, minimum_direct,
                                  minimum_range, out)
        : -1;
    pb_http_response_free(&response);
    return result;
}