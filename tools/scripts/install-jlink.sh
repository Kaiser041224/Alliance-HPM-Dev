#!/usr/bin/env bash
set -euo pipefail

# -------------------------------
# Config (can be overridden by env)
# -------------------------------
VERSION="${VERSION:-${1:-V8.40}}"
ROOT="${ROOT:-/workspace/tools}"
DST="${ROOT}/jlink-${VERSION}"
INSTALL_DIR="${DST}/install"
ENVSH="${DST}/env.jlink.sh"
PAGE_URL="https://www.segger.com/downloads/jlink/"

log() { echo "[jlink-installer] $*"; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    log "ERROR: missing command: $1"
    exit 1
  }
}

need_cmd curl
need_cmd tar
need_cmd python3
need_cmd find
need_cmd cp
need_cmd mktemp

ARCH="$(uname -m)"
case "${ARCH}" in
  x86_64) ARCH_KEY="x86_64" ;;
  aarch64) ARCH_KEY="aarch64" ;;
  *)
    log "Unsupported architecture: ${ARCH}"
    exit 1
    ;;
esac

VERSION_TOKEN="$(echo "${VERSION}" | tr '[:lower:]' '[:upper:]' | tr -d '.')"
if [[ "${VERSION_TOKEN}" != V* ]]; then
  VERSION_TOKEN="V${VERSION_TOKEN}"
fi

tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

log "Fetching download page: ${PAGE_URL}"
curl -fsSL -A "Mozilla/5.0" "${PAGE_URL}" -o "${tmp}/jlink_downloads.html"

download_url="$(
python3 - "${tmp}/jlink_downloads.html" "${VERSION_TOKEN}" "${ARCH_KEY}" <<'PY'
import re
import sys
from html import unescape
from urllib.parse import urljoin

html_path, version_token, arch_key = sys.argv[1], sys.argv[2], sys.argv[3]
base = "https://www.segger.com/downloads/jlink/"

with open(html_path, "r", encoding="utf-8", errors="ignore") as f:
    html = f.read()

hrefs = re.findall(r'href=["\']([^"\']+)["\']', html, flags=re.IGNORECASE)
links = [unescape(x.strip()) for x in hrefs]

def looks_like_linux_jlink(url: str) -> bool:
    u = url.lower()
    if "jlink_linux" not in u:
        return False
    if arch_key not in u:
        return False
    if version_token.lower() not in u:
        return False
    return u.endswith(".deb") or u.endswith(".tgz") or u.endswith(".tar.gz") or u.endswith(".tar.xz")

candidates = []
for link in links:
    full = urljoin(base, link)
    if looks_like_linux_jlink(full):
        candidates.append(full)

def score(url: str) -> int:
    u = url.lower()
    s = 0
    if ".deb" in u:
        s += 6
    if ".tar.xz" in u:
        s += 5
    if ".tar.gz" in u or ".tgz" in u:
        s += 4
    if "64" in u:
        s += 1
    if "arm" in u and arch_key == "aarch64":
        s += 2
    return s

candidates = sorted(set(candidates), key=score, reverse=True)
print(candidates[0] if candidates else "")
PY
)"

if [[ -z "${download_url}" ]]; then
  log "ERROR: No matching Linux package found for ${VERSION} (${ARCH_KEY})."
  log "Try opening ${PAGE_URL} manually and check asset naming."
  exit 1
fi

archive="${tmp}/jlink.pkg"
log "Downloading: ${download_url}"
# SEGGER requires terms acceptance via POST before returning the binary payload
curl -fL -A "Mozilla/5.0" -X POST \
  -d "accept_license_agreement=accepted&submit=Download+software" \
  "${download_url}" -o "${archive}"

if head -c 512 "${archive}" | LC_ALL=C tr -d "\000" | grep -qi "<!DOCTYPE html"; then
  log "ERROR: SEGGER returned HTML instead of package. Manual license flow may be required."
  exit 1
fi

rm -rf "${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}" "${DST}"

if [[ "${download_url}" == *.deb ]]; then
  need_cmd dpkg-deb
  log "Extracting .deb package..."
  dpkg-deb -x "${archive}" "${tmp}/extract"
  jlink_root="$(find "${tmp}/extract" -type d -path "*/opt/SEGGER/JLink*" | head -n 1 || true)"
  if [[ -z "${jlink_root}" ]]; then
    log "ERROR: could not find J-Link payload under /opt/SEGGER in package."
    exit 1
  fi
  cp -a "${jlink_root}/." "${INSTALL_DIR}/"
else
  log "Extracting archive package..."
  mkdir -p "${tmp}/extract"
  tar -xf "${archive}" -C "${tmp}/extract"
  jlink_exe="$(find "${tmp}/extract" -type f -name JLinkExe -print | head -n 1 || true)"
  if [[ -z "${jlink_exe}" ]]; then
    log "ERROR: JLinkExe not found in extracted archive."
    exit 1
  fi
  jlink_root="$(dirname "${jlink_exe}")"
  cp -a "${jlink_root}/." "${INSTALL_DIR}/"
fi

cat > "${ENVSH}" <<EOF
# SEGGER J-Link (${VERSION})
export JLINK_ROOT="${INSTALL_DIR}"
export PATH="${INSTALL_DIR}:\$PATH"
export LD_LIBRARY_PATH="${INSTALL_DIR}:\${LD_LIBRARY_PATH:-}"
EOF
chmod +x "${ENVSH}"

log "Installation complete: ${INSTALL_DIR}"
if [[ -x "${INSTALL_DIR}/JLinkExe" ]]; then
  "${INSTALL_DIR}/JLinkExe" -? 2>/dev/null | head -n 1 || true
else
  log "WARNING: JLinkExe not found at expected path: ${INSTALL_DIR}/JLinkExe"
fi

echo
echo "To use:"
echo "  source ${ENVSH}"
echo "  JLinkExe -?"
