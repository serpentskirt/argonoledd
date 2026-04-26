# argonoledd

A C daemon that displays real-time system stats and custom messages on a Argon Industria OLED display via I2C. Inspired by [argononed](https://gitlab.com/DarkElvenAngel/argononed) and [argoneonoled](https://github.com/JeffCurless/argoneon).

Cycles through configurable screens: clock, CPU usage, storage, bandwidth, RAID status, available RAM, temperature and IP address.

Custom messages can be sent over the Unix socket, so scripts and deploy hooks can briefly take over the display.

---

## Requirements

- Linux SBC with I2C enabled (`/dev/i2c-1`)
- SSD1306-based 128×64 OLED wired to I2C bus 1, address `0x3C`
- `gcc`, `make`
- `mdadm` (optional, for RAID screen)

---

## I2C setup (Raspberry Pi)

If `/dev/i2c-1` is missing, enable I2C:

```bash
sudo raspi-config nonint do_i2c 0
```

And make sure `/boot/firmware/config.txt` contains:
```
dtparam=i2c_arm=on
```

Then reboot and re-run `./install`.

---

## Install

```bash
git clone <repo>
cd argonoledd
sudo ./install
sudo usermod -aG argonoledd $USER
newgrp argonoledd
```

The script checks dependencies, builds the binary, installs everything, and starts the systemd service.

**What gets installed:**

| Path | Contents |
|------|----------|
| `/usr/local/sbin/argonoledd` | Binary |
| `/usr/local/share/argonoledd/res/` | Font and background image assets |
| `/etc/argonoledd.conf` | Config file (not overwritten on reinstall) |
| `/etc/systemd/system/argonoledd.service` | Systemd unit |

To uninstall:
```bash
sudo make uninstall
```

Config at `/etc/argonoledd.conf` is left in place.

---

## Configuration

`/etc/argonoledd.conf` - INI format:
```ini
[OLED]
# Seconds each screen is shown before rotating
screenduration = 3

# Screen order - remove names to disable screens
screenlist = clock cpu storage ram temp ip
```

Valid screen names: `clock` `cpu` `storage` `bandwidth` `raid` `ram` `temp` `ip`

Changes take effect on service restart:
```bash
sudo systemctl restart argonoledd
```

---

## Running manually

```bash
./argonoledd --foreground          # logs to stderr
./argonoledd --foreground --config /path/to/custom.conf
./argonoledd --help
```

---

## Socket commands

The daemon listens on `/run/argonoledd.sock`. Any process in the `argonoledd` group can send one-line commands using Python:
```bash
# Check the daemon is running
python3 -c 'import socket; s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect("/run/argonoledd.sock"); s.sendall(b"PING\n"); print(s.recv(64).decode(), end="")'

# Show a message on screen for one screenduration interval
python3 -c 'import socket; s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect("/run/argonoledd.sock"); s.sendall(b"MESSAGE header=deploy status=ok pods=3/3\n")'

# Trigger a full status rotation (all screens once, then blank)
python3 -c 'import socket; s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect("/run/argonoledd.sock"); s.sendall(b"STATUS\n")'
```

`MESSAGE` fields: `header=` (up to 14 chars, shown large); any other `key=val` pairs become detail lines (6 maximum, truncated to 18 chars each). After `screenduration` seconds the display blanks.

---

## Logs

```bash
journalctl -u argonoledd -f
```

---

## Build targets

```bash
make              # build optimized binary
make asan         # build with AddressSanitizer
make install      # install (requires root)
make update       # rebuild and redeploy without touching config
make uninstall    # remove everything except config
make clean        # remove build artifacts
```

---

## Hardware notes

- Display: SSD1306 128×64, I2C address `0x3C`
- Bus: `/dev/i2c-1` (hardcoded)
- Assets: font bitmaps and background images in `res/` - reused `argoneonoled` Python library assets
