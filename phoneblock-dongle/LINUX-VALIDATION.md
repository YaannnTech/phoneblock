# Linux dongle validation

This checklist covers the first real-network test of the native Linux port.
Run the service on a Raspberry Pi or Debian host in the same LAN as the
Fritz!Box. Do not expose the SIP or status ports to the Internet.

## Install

Install the Debian package and create the configuration:

```bash
sudo apt install ./phoneblock-dongle_0.1.0_arm64.deb
sudoedit /etc/phoneblock/dongle.conf
```

Required settings:

```ini
sip_host=fritz.box
sip_port=5060
sip_user=620
sip_pass=<Fritz!Box IP-phone password>
phoneblock_base_url=https://phoneblock.net/phoneblock
phoneblock_token=pbt_<token>
announcement_path=/var/lib/phoneblock/announcement.alaw
```

The announcement must be raw, mono, 8 kHz G.711 A-law data. A 20-second test
file is 160,000 bytes. Keep the file owned and readable by `phoneblock`:

```bash
sudo install -o phoneblock -g phoneblock -m 0640 announcement.alaw \
  /var/lib/phoneblock/announcement.alaw
```

## Preflight

Run these before enabling systemd:

```bash
sudo -u phoneblock /usr/bin/phoneblock-dongle \
  --config /etc/phoneblock/dongle.conf --check-config

sudo -u phoneblock /usr/bin/phoneblock-dongle \
  --config /etc/phoneblock/dongle.conf --probe-sip

sudo -u phoneblock /usr/bin/phoneblock-dongle \
  --config /etc/phoneblock/dongle.conf --register-sip
```

Expected results are a successful UDP bind, then SIP `200 OK` after the Digest
challenge. A failure at this stage is usually a Fritz!Box extension/port,
credential, or LAN firewall issue.

## Service test

```bash
sudo systemctl enable --now phoneblock-dongle
sudo systemctl status phoneblock-dongle
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/api/status
journalctl -u phoneblock-dongle -f
```

The service should stay registered, refresh the registration after 30 minutes,
and keep the status endpoint responsive while the SIP listener is active.

## Call test

1. Call the configured Fritz!Box extension from another phone.
2. Confirm the service receives the INVITE and checks the caller through
   PhoneBlock.
3. For a known spam number, confirm the service returns `200 OK`, waits for
   ACK, and streams PCMA RTP to the caller's SDP endpoint.
4. Confirm the announcement stops on remote BYE and the service remains ready
   for a subsequent call.
5. For a non-spam or failed lookup, confirm the service returns `486 Busy Here`.

## Current limitations

- Linux transport currently supports UDP SIP only.
- The Linux implementation supports one active dialog.
- TR-064 authentication/provisioning is not yet wired into the executable.
- The Linux HTTP surface is a health/status endpoint, not the ESP32 dashboard.