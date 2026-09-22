# Debian packaging

The package installs the native Linux executable as
`/usr/bin/phoneblock-dongle` and provides a hardened systemd unit.

The Linux host build uses the distribution `libcjson-dev` package and does
not require ESP-IDF.

Build from the `phoneblock-dongle` directory:

```bash
dpkg-buildpackage -us -uc
```

After installation, create `/etc/phoneblock/dongle.conf`, then enable the
service:

```bash
sudo systemctl enable --now phoneblock-dongle
```