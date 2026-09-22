#pragma once

// Send one unauthenticated REGISTER and inspect the registrar response.
// Returns 0 when a response was received, -1 on transport or build failure.
// `status` receives the SIP status code and `challenge` receives a parsed
// Digest challenge when the registrar returned 401 or 407.
int pb_linux_sip_register_probe(const char *host, int port,
                                const char *user, const char *password,
                                int local_port,
                                int *status, char *challenge,
                                int challenge_cap);