# sinsebeolsik-p2-fcitx5

**Sinsebeolsik P2 (신세벌식 P2)** Hangul input method engine for fcitx5 on Linux.

[한국어 README](README.ko.md)

## Install — .deb (recommended)

Grab the latest `.deb` from [Releases](https://github.com/Bogamie/sinsebeolsik-p2-fcitx5/releases/latest):

```bash
# example: v0.1.3
wget https://github.com/Bogamie/sinsebeolsik-p2-fcitx5/releases/download/v0.1.3/fcitx5-sinsebeolsik-p2_0.1.3_amd64.deb
sudo apt install ./fcitx5-sinsebeolsik-p2_0.1.3_amd64.deb

sinsebeolsik-p2-setup-trigger   # one-time: register Hangul key as fcitx5 trigger
fcitx5-remote -r                # restart fcitx5
```

Then open `fcitx5-configtool`, add **Sinsebeolsik P2** under **Current Input Methods**. Toggle with the Hangul key.

## Install — from source

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
    libfcitx5core-dev libfmt-dev libtomlplusplus-dev gettext pkg-config

git clone https://github.com/Bogamie/sinsebeolsik-p2-fcitx5.git
cd sinsebeolsik-p2-fcitx5
./scripts/dev-reload.sh                 # build + install + restart fcitx5
./scripts/setup-fcitx5-trigger.sh       # one-time: register Hangul key
```

Build a `.deb` directly:

```bash
sudo apt install devscripts debhelper rsync
./packaging/build-deb.sh
sudo dpkg -i fcitx5-sinsebeolsik-p2_*.deb
```

## Customizing the keymap

Search order (first match wins):

1. `$SIN3P2_KEYMAP` env var
2. `$XDG_CONFIG_HOME/sinsebeolsik-p2/sinsebeolsik_p2.toml`
3. `~/.config/sinsebeolsik-p2/sinsebeolsik_p2.toml`
4. `~/.local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
5. `/usr/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
6. `/usr/local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
7. Built-in fallback

See [docs/adding-a-keymap.md](docs/adding-a-keymap.md) for the schema.

## Documentation

- Architecture: [docs/architecture.md](docs/architecture.md)
- Totem 38 firmware: [docs/totem-firmware.md](docs/totem-firmware.md)
- Adding a new keymap: [docs/adding-a-keymap.md](docs/adding-a-keymap.md)

## Contributing

Issues and PRs welcome. For new layouts, follow the verification workflow in [docs/adding-a-keymap.md](docs/adding-a-keymap.md). Sign commits with `git commit -s` (DCO).

## License

MIT — see `LICENSE`.

## Acknowledgements

- [pat.im](https://pat.im/1136) — Sinsebeolsik P2 spec
- [pat.im simulator](https://ohi.pat.im/?ko=sin3-p2) — behavior oracle
- [fcitx/fcitx5](https://github.com/fcitx/fcitx5)
- [Riey/kime](https://github.com/Riey/kime), [libhangul](https://github.com/libhangul/libhangul)
