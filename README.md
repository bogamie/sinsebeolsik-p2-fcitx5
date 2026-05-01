# sinsebeolsik-p2-fcitx5

**Sinsebeolsik P2 (신세벌식 P2)** Hangul input method engine for fcitx5 on Linux.

First-class support for the Totem 38-key split keyboard, with graceful fallback to standard 60% / TKL / full-size layouts.

[Korean README (한국어)](README.ko.md) — primary; this English version mirrors it.

## Highlights

- **Simulator-verified P2 behavior** — the [pat.im P2 simulator](https://ohi.pat.im/?ko=sin3-p2) is the oracle. The engine encodes P2's non-obvious rules: no dokkaebibul (도깨비불), no auto-IEUNG injection, no jong-cluster splitting on next vowel, distinct virtual-vs-real jung, and ssang-jaeum via doubled cho keystroke.
- **Keyboard / firmware agnostic** — the engine reads KeySyms from fcitx5, not raw scancodes. Totem (QMK), Vial, or stock XKB on a generic 60% all work.
- **TOML keymap** — externalized to `keymaps/sinsebeolsik_p2.toml`. Embedded at build time; users can drop an override at standard search-path locations.
- **141 unit tests** — Catch2-based. Covers the automaton, jamo composition, and TOML keymap translation, all locked against simulator-verified sequences.

## Status

Alpha. Validated for daily Hangul typing on Ubuntu 24.04 LTS. Forward compatibility with Ubuntu 26.04 LTS is a stated goal and will be addressed once 26.04 is released.

## Build from source

### Dependencies

```bash
sudo apt install \
    build-essential cmake ninja-build \
    extra-cmake-modules libfcitx5core-dev libfmt-dev \
    libtomlplusplus-dev gettext pkg-config
```

If `libtomlplusplus-dev` isn't installed, CMake falls back to FetchContent and pulls v3.4.0 from upstream (requires network at configure time).

### One-shot build + install + fcitx5 restart

```bash
git clone https://github.com/Bogamie/sinsebeolsik-p2-fcitx5.git
cd sinsebeolsik-p2-fcitx5
./scripts/dev-reload.sh
```

Installs into `~/.local/lib/fcitx5/`, `~/.local/share/fcitx5/addon/`, and `~/.local/share/fcitx5/sinsebeolsik-p2/`, then restarts fcitx5 with `FCITX_ADDON_DIRS` pointing at the user dir.

### Register the Hangul key as IM trigger (one-time)

If you installed via `.deb` (v0.1.1+):
```bash
sinsebeolsik-p2-setup-trigger
fcitx5-remote -r
```

If you built from source:
```bash
./scripts/setup-fcitx5-trigger.sh
fcitx5-remote -r
```

Both forms idempotently append `Hangul` (KeySym 0xff31) under `[Hotkey/TriggerKeys]` in `~/.config/fcitx5/config`. Safe to re-run.

### Add the IM in fcitx5-configtool

1. Launch `fcitx5-configtool`
2. Add **Sinsebeolsik P2** (or **신세벌식 P2**) under **Current Input Methods**
3. Apply

Press the Hangul key to toggle Korean input; type using the Sinsebeolsik P2 layout.

## .deb package

```bash
sudo apt install devscripts debhelper rsync
./packaging/build-deb.sh
sudo dpkg -i fcitx5-sinsebeolsik-p2_0.1.0_*.deb
```

See `packaging/debian/control` for full build dependency declarations.

## Configuration

Default keymap search path (first match wins):

1. `$SIN3P2_KEYMAP` environment variable (developer override)
2. `$XDG_CONFIG_HOME/sinsebeolsik-p2/sinsebeolsik_p2.toml`
3. `~/.config/sinsebeolsik-p2/sinsebeolsik_p2.toml`
4. `~/.local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
5. `/usr/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
6. `/usr/local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
7. Built-in fallback (compiled into the addon)

To customize, drop a modified TOML at any of these locations. See [docs/adding-a-keymap.md](docs/adding-a-keymap.md) for the schema.

## Documentation

- Korean: [README.ko.md](README.ko.md)
- Architecture: [docs/architecture.md](docs/architecture.md)
- Totem 38 firmware notes: [docs/totem-firmware.md](docs/totem-firmware.md)
- Adding a new keymap: [docs/adding-a-keymap.md](docs/adding-a-keymap.md)
- Project goals & milestones: [CLAUDE.md](CLAUDE.md)

## English layout?

Out of scope for this engine. Recommended setup:

- Use Canary (or Dvorak / Colemak / your preference) at the **XKB** layer for English
- This engine handles only Hangul composition while activated

The IBus-hangul style of bundling Hangul/English toggle inside a single engine is intentionally avoided — the system-layer split is cleaner and survives session changes better.

## Contributing

- Issues and PRs are welcome.
- For new Hangul layouts, follow the verification workflow in [docs/adding-a-keymap.md](docs/adding-a-keymap.md): simulator → test cases → code.
- Sign commits with `git commit -s` (DCO).

## License

MIT — see `LICENSE`.

## Acknowledgements

- [pat.im](https://pat.im/1136) — author of the Sinsebeolsik P2 specification
- [pat.im simulator](https://ohi.pat.im/?ko=sin3-p2) — behavior oracle for this engine
- [fcitx/fcitx5](https://github.com/fcitx/fcitx5) — the input method framework
- [Riey/kime](https://github.com/Riey/kime) — Rust Hangul IME, automaton structure reference
- [libhangul](https://github.com/libhangul/libhangul) — canonical Hangul composition reference
