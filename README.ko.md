# sinsebeolsik-p2-fcitx5

리눅스 fcitx5용 신세벌식 P2 한글 입력기.

토템(Totem) 38키 분할 키보드를 일등 시민으로 지원하며, 표준 60% / TKL /
풀 사이즈 키보드는 보조 대상으로 지원합니다.

## 상태

알파 이전 단계. 현재 M1(스캐폴딩) 진행 중. 아직 사용할 수 없습니다.
마일스톤은 `CLAUDE.md` 참고.

## 빠른 시작

작성 예정.

## 문서

- 영어: `README.md`
- 아키텍처: `docs/architecture.md` (작성 예정)
- 토템 38 펌웨어 가이드: `docs/totem-firmware.md` (작성 예정)
- 새 키맵 추가: `docs/adding-a-keymap.md` (작성 예정)

영어 입력 전환은 본 엔진 범위 밖입니다 — 권장 구성은 XKB 레이어에서
Canary 배열을 사용하고, 본 엔진은 토글된 동안의 한글 조합만 담당합니다.

## 라이선스

MIT — `LICENSE` 참고.
