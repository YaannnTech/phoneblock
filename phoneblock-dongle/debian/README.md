# Debian packaging

The package installs the native Linux executable as
`/usr/bin/phoneblock-dongle` and provides a hardened systemd unit.

The current development build still obtains cJSON from ESP-IDF through the
host-test Makefile. Before building a standalone Debian package, vendor cJSON
or change the Linux build to use the distribution `libcjson-dev` package.

Build from the `phoneblock-dongle` directory:

```bash
dpkg-buildpackage -us -uc
```

After installation, create `/etc/phoneblock/dongle.conf`, then enable the
service:

```bash
sudo systemctl enable --now phoneblock-dongle
```