# sinsebeolsik-p2-fcitx5

리눅스 fcitx5용 **신세벌식 P2** 한글 입력기.

[English README](README.md)

## 설치 — .deb (권장)

[Releases](https://github.com/Bogamie/sinsebeolsik-p2-fcitx5/releases/latest)에서 최신 `.deb`을 받아 설치:

```bash
# 예: v0.1.3
wget https://github.com/Bogamie/sinsebeolsik-p2-fcitx5/releases/download/v0.1.3/fcitx5-sinsebeolsik-p2_0.1.3_amd64.deb
sudo apt install ./fcitx5-sinsebeolsik-p2_0.1.3_amd64.deb

sinsebeolsik-p2-setup-trigger   # Hangul 키를 fcitx5 트리거로 등록 (1회)
fcitx5-remote -r                # fcitx5 재시작
```

이어서 `fcitx5-configtool`을 열고 **현재 입력 방식**에 `신세벌식 P2`를 추가. Hangul 키로 한/영 토글.

## 설치 — 소스 빌드

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
    libfcitx5core-dev libfmt-dev libtomlplusplus-dev gettext pkg-config

git clone https://github.com/Bogamie/sinsebeolsik-p2-fcitx5.git
cd sinsebeolsik-p2-fcitx5
./scripts/dev-reload.sh                 # 빌드+설치+fcitx5 재시작
./scripts/setup-fcitx5-trigger.sh       # Hangul 키 등록 (1회)
```

`.deb` 직접 빌드:

```bash
sudo apt install devscripts debhelper rsync
./packaging/build-deb.sh
sudo dpkg -i fcitx5-sinsebeolsik-p2_*.deb
```

## 키맵 커스터마이즈

검색 순서 (먼저 발견되는 것 사용):

1. `$SIN3P2_KEYMAP` 환경변수
2. `$XDG_CONFIG_HOME/sinsebeolsik-p2/sinsebeolsik_p2.toml`
3. `~/.config/sinsebeolsik-p2/sinsebeolsik_p2.toml`
4. `~/.local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
5. `/usr/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
6. `/usr/local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
7. 빌드 시 임베드된 fallback

형식은 [docs/adding-a-keymap.md](docs/adding-a-keymap.md) 참고.

## 문서

- 아키텍처: [docs/architecture.md](docs/architecture.md)
- 토템 38 펌웨어: [docs/totem-firmware.md](docs/totem-firmware.md)
- 새 키맵 추가: [docs/adding-a-keymap.md](docs/adding-a-keymap.md)

## 기여

Issue / PR 환영. 새 배열 제안 시 [docs/adding-a-keymap.md](docs/adding-a-keymap.md)의 검증 절차를 따라주세요. 커밋은 `git commit -s`로 DCO sign-off 포함.

## 라이선스

MIT — `LICENSE` 참고.

## 참고 / 감사

- [pat.im](https://pat.im/1136) — 신세벌식 P2 사양
- [pat.im 시뮬레이터](https://ohi.pat.im/?ko=sin3-p2) — 동작 oracle
- [fcitx/fcitx5](https://github.com/fcitx/fcitx5)
- [Riey/kime](https://github.com/Riey/kime), [libhangul](https://github.com/libhangul/libhangul)
