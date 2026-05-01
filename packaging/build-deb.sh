#!/usr/bin/env bash
# .deb 빌드 — packaging/debian 을 프로젝트 루트로 복사한 worktree에서 debuild.
#
# 표준 Debian 도구는 `debian/` 디렉토리가 source root에 있어야 작동한다.
# 본 프로젝트는 `packaging/debian/` 으로 격리되어 있어, 이 스크립트는
# 임시 worktree를 만들어 거기서 빌드한다.
#
# 사용:
#   ./packaging/build-deb.sh           # 빌드
#   ./packaging/build-deb.sh --keep    # 빌드 후 worktree 보존 (디버깅)
#
# 결과: $ROOT/*.deb (parent dir of worktree)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${ROOT}/build-deb-tree"
KEEP=0

case "${1:-}" in
    --keep) KEEP=1 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    "") ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
esac

# 의존성 확인
for tool in debuild dpkg-buildpackage rsync; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "필요한 도구 없음: $tool" >&2
        echo "  sudo apt install devscripts dpkg-dev rsync debhelper" >&2
        exit 1
    }
done

# 깨끗한 worktree
rm -rf "${WORK}"
mkdir -p "${WORK}"

# 소스 복사 (빌드 산물, .git, packaging은 제외 — debian/은 root로 따로 복사)
rsync -a \
    --exclude='build*' \
    --exclude='.git' \
    --exclude='packaging' \
    --exclude='.omc' \
    --exclude='*.deb' \
    "${ROOT}/" "${WORK}/"

# debian/ 을 root에 배치
cp -r "${ROOT}/packaging/debian" "${WORK}/debian"
chmod +x "${WORK}/debian/rules"

cd "${WORK}"
debuild -us -uc -b

cd "${ROOT}"
echo
echo "✓ 빌드 완료. .deb 위치:"
ls -1 "${ROOT}"/*.deb 2>/dev/null || echo "  (없음 — debuild 출력 확인)"

if [[ "${KEEP}" -eq 0 ]]; then
    rm -rf "${WORK}"
else
    echo "worktree 보존: ${WORK}"
fi
