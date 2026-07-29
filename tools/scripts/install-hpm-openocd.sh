#!/usr/bin/env bash
set -euo pipefail

# -------------------------------
# Fixed configuration
# -------------------------------
TAG="hpm_xpi_v0.4.0"
REPO="hpmicro/riscv-openocd"
ROOT="/workspace/tools"
DST="${ROOT}/openocd-hpm"
INSTALL_DIR="${DST}/install"
ENVSH="${DST}/env.openocd.sh"

log(){ echo "[hpm-openocd] $*"; }

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
  x86_64|aarch64) ;;
  *)
    log "Unsupported architecture: ${ARCH}"
    exit 1
    ;;
esac

tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

API="https://api.github.com/repos/${REPO}/releases/tags/${TAG}"

log "Fetching release metadata (${TAG})..."
curl -fsSL "${API}" -o "${tmp}/release.json"

# -------------------------------
# Select correct Linux asset
# -------------------------------
url="$(
python3 - "${ARCH}" "${tmp}/release.json" <<'PY'
import json,sys,os
arch = sys.argv[1]
path = sys.argv[2]

with open(path, "r", encoding="utf-8") as f:
    data = json.load(f)

assets = data.get("assets", [])

def match(name: str) -> bool:
    n = name.lower()
    if "linux" not in n:
        return False
    # assets 命名不一定包含 openocd 字样，所以这里不强制包含 "openocd"
    if not (n.endswith(".tar.gz") or n.endswith(".tgz") or n.endswith(".tar.xz")):
        return False

    if arch == "x86_64":
        # reject arm builds
        if "aarch64" in n or "arm64" in n:
            return False
        # accept if contains x86_64/amd64 OR no arch string
        return True

    if arch == "aarch64":
        # prefer arm build
        if "aarch64" in n or "arm64" in n:
            return True
        # if no arch string at all, still allow as fallback
        return False

    return False

candidates = [a for a in assets if match(a.get("name",""))]

def score(a):
    name = a.get("name","").lower()
    s = 0
    if name.endswith(".tar.xz"): s += 3
    if name.endswith(".tar.gz") or name.endswith(".tgz"): s += 2
    if arch == "aarch64" and ("aarch64" in name or "arm64" in name): s += 5
    if arch == "x86_64" and ("x86_64" in name or "amd64" in name): s += 2
    return s

candidates.sort(key=score, reverse=True)

if candidates:
    print(candidates[0].get("browser_download_url",""))
else:
    print("")
PY
)"

if [[ -z "${url}" ]]; then
  log "ERROR: Could not find suitable Linux binary in release ${TAG}."
  log "Available assets:"
  python3 - "${tmp}/release.json" <<'PY'
import json,sys
path=sys.argv[1]
data=json.load(open(path,"r",encoding="utf-8"))
for a in data.get("assets",[]):
    print(" -", a.get("name",""))
PY
  exit 1
fi

log "Downloading: ${url}"
curl -fL "${url}" -o "${tmp}/openocd.tar"

log "Extracting..."
mkdir -p "${tmp}/extract"
tar -xf "${tmp}/openocd.tar" -C "${tmp}/extract"

# -------------------------------
# Locate prefix containing bin/openocd
# -------------------------------
openocd_path="$(find "${tmp}/extract" -type f -path "*/bin/openocd" -print | head -n 1 || true)"
if [[ -z "${openocd_path}" ]]; then
  log "ERROR: openocd binary not found in archive after extraction."
  log "Candidates:"
  find "${tmp}/extract" -maxdepth 6 -type f -name '*openocd*' -print | head -n 80 || true
  exit 1
fi

prefix="$(dirname "$(dirname "${openocd_path}")")"

log "Installing to ${INSTALL_DIR}"
rm -rf "${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}"
cp -a "${prefix}/." "${INSTALL_DIR}/"

# -------------------------------
# Create env file
# -------------------------------
mkdir -p "${DST}"
cat > "${ENVSH}" <<EOF
# HPM OpenOCD (${TAG})
export PATH="${INSTALL_DIR}/bin:\$PATH"
export OPENOCD_SCRIPTS="${INSTALL_DIR}/share/openocd/scripts"
EOF
chmod +x "${ENVSH}"

log "Installation complete."
"${INSTALL_DIR}/bin/openocd" --version | head -n 2 || true

echo
echo "To use:"
echo "  source ${ENVSH}"
echo "  openocd --version"