# sinsebeolsik-p2-fcitx5

리눅스 fcitx5용 **신세벌식 P2** 한글 입력기.

토템(Totem) 38키 분할 키보드를 일등 시민으로 지원하며, 표준 60% / TKL / 풀사이즈 키보드도 보조 대상으로 지원합니다.

[English README](README.md)

## 특징

- **시뮬레이터-검증된 P2 동작** — pat.im의 [P2 시뮬레이터](https://ohi.pat.im/?ko=sin3-p2)가 oracle. 도깨비불 없음, 자동 ㅇ 보충 없음, 받침 클러스터 분할 없음, 가상-실제 중성 구분 등 P2 특유의 비-자명한 규칙을 그대로 인코딩.
- **한 자/펌웨어 추상화** — 키보드(토템/표준)와 펌웨어(QMK/기본 XKB) 무관. 엔진은 fcitx5에서 받은 KeySym만 처리.
- **TOML 키맵** — `keymaps/sinsebeolsik_p2.toml`에 외화. 빌드 시 임베드되고 사용자가 동일 위치에 파일을 떨궈 override 가능.
- **141개 단위 테스트** — Catch2 기반. 자동기·자모 합성·키맵 변환 모두 시뮬레이터 케이스로 lock-in.

## 상태

알파. 우분투 24.04 LTS에서 일상 타이핑 검증된 수준이며, 실제 사용 가능. 우분투 26.04 LTS 호환은 단계적으로 시도 예정.

## 설치 — 소스에서 빌드

### 의존성

```bash
sudo apt install \
    build-essential cmake ninja-build \
    extra-cmake-modules libfcitx5core-dev libfmt-dev \
    libtomlplusplus-dev gettext pkg-config
```

`libtomlplusplus-dev`가 없으면 CMake가 자동으로 FetchContent로 v3.4.0을 받아옵니다(인터넷 필요).

### 빌드 + 설치 + fcitx5 재시작 (한방 스크립트)

```bash
git clone https://github.com/Bogamie/sinsebeolsik-p2-fcitx5.git
cd sinsebeolsik-p2-fcitx5
./scripts/dev-reload.sh
```

스크립트가 `~/.local/lib/fcitx5/`, `~/.local/share/fcitx5/addon/`, `~/.local/share/fcitx5/sinsebeolsik-p2/`에 설치하고 `FCITX_ADDON_DIRS`를 잡아 fcitx5를 재시작합니다.

### Hangul 키 트리거 등록 (1회)

```bash
./scripts/setup-fcitx5-trigger.sh
fcitx5-remote -r
```

`~/.config/fcitx5/config`의 `[Hotkey/TriggerKeys]`에 `Hangul`(KeySym 0xff31)을 멱등적으로 추가합니다. 이미 있으면 no-op.

### fcitx5-configtool에서 IM 추가

1. `fcitx5-configtool` 실행
2. **현재 입력 방식**에 `Sinsebeolsik P2`(또는 `신세벌식 P2`) 추가
3. 적용

이제 Hangul 키로 한/영 토글, 입력 시 신세벌식 P2 글쇠 배치대로 한글 합성.

## .deb 패키지

```bash
sudo apt install devscripts debhelper rsync
./packaging/build-deb.sh
sudo dpkg -i fcitx5-sinsebeolsik-p2_0.1.0_*.deb
```

자세한 빌드 설명은 `packaging/debian/control` 참고.

## 사용

기본 키맵 위치 (검색 순서):

1. `$SIN3P2_KEYMAP` 환경변수 (개발자 override)
2. `$XDG_CONFIG_HOME/sinsebeolsik-p2/sinsebeolsik_p2.toml`
3. `~/.config/sinsebeolsik-p2/sinsebeolsik_p2.toml`
4. `~/.local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
5. `/usr/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
6. `/usr/local/share/fcitx5/sinsebeolsik-p2/sinsebeolsik_p2.toml`
7. (위 어느 것도 없으면) 빌드 시 임베드된 fallback

자기 입맛대로 키맵을 고치려면 위 위치 중 하나에 수정본을 두면 됩니다. 형식은 [docs/adding-a-keymap.md](docs/adding-a-keymap.md) 참고.

## 문서

- 영어: [README.md](README.md)
- 아키텍처: [docs/architecture.md](docs/architecture.md)
- 토템 38 펌웨어: [docs/totem-firmware.md](docs/totem-firmware.md)
- 새 키맵 추가: [docs/adding-a-keymap.md](docs/adding-a-keymap.md)
- 프로젝트 마일스톤·결정사항: [CLAUDE.md](CLAUDE.md)

## 영어 입력은?

본 엔진의 범위 밖입니다. 권장 구성:

- XKB 레이어에서 Canary(또는 Dvorak/Colemak) 배열 사용
- 본 엔진은 토글된 동안의 한글 조합만 담당

영어와 한글을 같은 엔진에서 토글하는 IBus-hangul 스타일 흐름은 의도적으로 채택하지 않았습니다 — 시스템 레이어 분리가 더 견고합니다.

## 기여

- Issue / PR 환영합니다.
- 새 한글 배열 제안 시 [docs/adding-a-keymap.md](docs/adding-a-keymap.md)의 검증 절차를 따라주세요.
- 커밋은 `git commit -s`로 DCO sign-off 포함.

## 라이선스

MIT — `LICENSE` 참고.

## 참고 / 감사

- [pat.im](https://pat.im/1136) — 신세벌식 P2 사양 작성자
- [pat.im 시뮬레이터](https://ohi.pat.im/?ko=sin3-p2) — 본 엔진의 동작 oracle
- [fcitx/fcitx5](https://github.com/fcitx/fcitx5) — 입력 프레임워크
- [Riey/kime](https://github.com/Riey/kime) — Rust Hangul IME, 자동기 구조 참고
- [libhangul](https://github.com/libhangul/libhangul) — 한글 조합 규칙 reference
