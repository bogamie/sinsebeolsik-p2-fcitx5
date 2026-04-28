# sinsebeolsik-p2-fcitx5

Sinsebeolsik P2 (신세벌식 P2) Hangul input method for fcitx5 on Linux.

First-class support for the Totem 38-key split keyboard, with graceful
fallback to standard 60% / TKL / full-size layouts.

## Status

Pre-alpha. Currently scaffolding (M1). Not usable yet. See `CLAUDE.md` for
the milestone roadmap.

## Quick start

TBD.

## Documentation

- Korean: `README.ko.md`
- Architecture: `docs/architecture.md` (TBD)
- Totem 38 firmware notes: `docs/totem-firmware.md` (TBD)
- Adding a new keymap: `docs/adding-a-keymap.md` (TBD)

English layout switching is intentionally out of scope for this engine —
recommended setup is the Canary layout at the XKB level, with this engine
only handling Hangul composition when toggled on.

## License

MIT — see `LICENSE`.
