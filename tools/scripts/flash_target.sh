#!/usr/bin/env bash
set -euo pipefail

# ---- Defaults (override via env or CLI args) ----
TOOL=""
ELF_FILE=""
JLINK_DEVICE=""
JLINK_IF="JTAG"
JLINK_SPEED="1000"
OPENOCD_BIN="${HPM_OPENOCD_PREFIX:-}/bin/openocd"
OPENOCD_SCRIPTS="${HPM_OCD_SCRIPTS:-${HPMDEV_SDK_DIR:-/workspace/sdk/hpm_sdk}/boards/openocd}"
OPENOCD_PROBE_CFG=""
OPENOCD_SOC_CFG=""
OPENOCD_BOARD_CFG=""
OPENOCD_BIN_ADDR="0x80003000"
OPENOCD_SPEED=""
OPENOCD_RISCV_TIMEOUT=""
DRY_RUN="0"

log() { echo "[flash] $*"; }
err() { echo "[flash] ERROR: $*" >&2; }

usage() {
  cat <<'EOF'
Usage:
  flash_target.sh --tool jlink  --device <MCU> --elf <file> [options]
  flash_target.sh --tool openocd --elf <file>  [options]

Required:
  --tool <openocd|jlink>   Flash tool
  --elf <path>             ELF/BIN file to flash

General:
  --dry-run                Print commands without executing
  -h, --help               Show this help

OpenOCD options:
  --openocd-bin <path>     OpenOCD binary   (default: $HPM_OPENOCD_PREFIX/bin/openocd)
  --ocd-scripts <dir>      Script directory  (default: $HPM_OCD_SCRIPTS)
  --probe-cfg <file>       Probe config      (e.g. probes/cmsis_dap.cfg)   [required]
  --soc-cfg <file>         SoC config        (e.g. soc/hpm5300.cfg)        [required]
  --board-cfg <file>       Board config      (e.g. boards/hpm5301evklite.cfg) [required]
  --bin-addr <addr>        Binary load addr  (default: 0x80003000, only for .bin)
  --ocd-speed <kHz>        Adapter speed     (DAPLink auto-retry if unset)
  --riscv-timeout <sec>    Command timeout   (DAPLink default: 60)

J-Link options:
  --device <name>          J-Link device name (e.g. HPM5301xEGx) [required]
  --if <JTAG|SWD>          Interface          (default: JTAG)
  --speed <kHz>            Speed              (default: 1000)

Examples:
  flash_target.sh --tool jlink --device HPM5301xEGx --elf build/output/demo.elf
  flash_target.sh --tool openocd --elf build/output/demo.elf \
    --probe-cfg probes/cmsis_dap.cfg \
    --soc-cfg soc/hpm5300.cfg \
    --board-cfg boards/hpm5301evklite.cfg
EOF
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    err "missing command: $1"
    exit 1
  }
}

run_openocd() {
  [[ -n "${OPENOCD_PROBE_CFG}" ]] || { err "--probe-cfg is required for openocd"; exit 1; }
  [[ -n "${OPENOCD_SOC_CFG}" ]]   || { err "--soc-cfg is required for openocd"; exit 1; }
  [[ -n "${OPENOCD_BOARD_CFG}" ]] || { err "--board-cfg is required for openocd"; exit 1; }

  local is_daplink="0"
  if [[ "${OPENOCD_PROBE_CFG}" == *"cmsis_dap"* || "${OPENOCD_PROBE_CFG}" == *"daplink"* ]]; then
    is_daplink="1"
  fi
  if [[ -z "${OPENOCD_RISCV_TIMEOUT}" && "${is_daplink}" == "1" ]]; then
    OPENOCD_RISCV_TIMEOUT="60"
  fi

  local program_cmd
  if [[ "${ELF_FILE}" == *.bin ]]; then
    program_cmd="program ${ELF_FILE} ${OPENOCD_BIN_ADDR} verify reset exit"
  else
    program_cmd="program ${ELF_FILE} verify reset exit"
  fi

  run_once() {
    local speed_arg="$1"
    local setup_local=""
    if [[ -n "${speed_arg}" ]]; then
      setup_local+="adapter speed ${speed_arg}; "
    fi
    if [[ -n "${OPENOCD_RISCV_TIMEOUT}" ]]; then
      setup_local+="riscv set_command_timeout_sec ${OPENOCD_RISCV_TIMEOUT}; "
    fi
    setup_local+="reset_config srst_only srst_nogate connect_deassert_srst; "
    setup_local+="init; reset halt; "

    local cmd=(
      "${OPENOCD_BIN}"
      -s "${OPENOCD_SCRIPTS}"
      -f "${OPENOCD_PROBE_CFG}"
      -f "${OPENOCD_SOC_CFG}"
      -f "${OPENOCD_BOARD_CFG}"
      -c "${setup_local}${program_cmd}"
    )

    log "tool=openocd"
    log "elf=${ELF_FILE}"
    log "cmd: ${cmd[*]}"
    if [[ "${DRY_RUN}" == "1" ]]; then
      return 0
    fi
    "${cmd[@]}"
  }

  if [[ "${is_daplink}" == "1" && -z "${OPENOCD_SPEED}" ]]; then
    local speeds=("1000" "800" "600" "400" "200")
    local max_attempts="${#speeds[@]}"
    local i
    for ((i=1; i<=max_attempts; i++)); do
      local s="${speeds[$((i-1))]}"
      log "DAPLink try ${i}/${max_attempts} at ${s} kHz"
      if run_once "${s}"; then
        return 0
      fi
      log "Attempt failed at ${s} kHz"
      sleep 1
    done
    return 1
  fi

  run_once "${OPENOCD_SPEED}"
}

run_jlink() {
  [[ -n "${JLINK_DEVICE}" ]] || { err "--device is required for jlink"; exit 1; }

  local jlink_bin="${JLINK_ROOT:-}/JLinkExe"
  if [[ ! -x "${jlink_bin}" ]]; then
    jlink_bin="JLinkExe"
  fi
  need_cmd "${jlink_bin}"

  local tmp_script
  tmp_script="$(mktemp)"
  trap "rm -f '${tmp_script}'" EXIT

  cat > "${tmp_script}" <<EOF
si ${JLINK_IF}
speed ${JLINK_SPEED}
connect
r
h
loadfile ${ELF_FILE}
r
g
qc
EOF

  local cmd=(
    "${jlink_bin}"
    -device "${JLINK_DEVICE}"
    -if "${JLINK_IF}"
    -speed "${JLINK_SPEED}"
    -autoconnect 1
    -CommanderScript "${tmp_script}"
  )

  log "tool=jlink"
  log "elf=${ELF_FILE}"
  log "cmd: ${cmd[*]}"
  if [[ "${DRY_RUN}" == "1" ]]; then
    return 0
  fi
  "${cmd[@]}"
}

# ---- Parse arguments ----
while [[ $# -gt 0 ]]; do
  case "$1" in
    --tool)          TOOL="${2:-}";                shift 2 ;;
    --elf)           ELF_FILE="${2:-}";            shift 2 ;;
    --dry-run)       DRY_RUN="1";                 shift ;;
    --openocd-bin)   OPENOCD_BIN="${2:-}";         shift 2 ;;
    --ocd-scripts)   OPENOCD_SCRIPTS="${2:-}";     shift 2 ;;
    --probe-cfg)     OPENOCD_PROBE_CFG="${2:-}";   shift 2 ;;
    --soc-cfg)       OPENOCD_SOC_CFG="${2:-}";     shift 2 ;;
    --board-cfg)     OPENOCD_BOARD_CFG="${2:-}";   shift 2 ;;
    --bin-addr)      OPENOCD_BIN_ADDR="${2:-}";    shift 2 ;;
    --ocd-speed)     OPENOCD_SPEED="${2:-}";       shift 2 ;;
    --riscv-timeout) OPENOCD_RISCV_TIMEOUT="${2:-}"; shift 2 ;;
    --device)        JLINK_DEVICE="${2:-}";        shift 2 ;;
    --if)            JLINK_IF="${2:-}";            shift 2 ;;
    --speed)         JLINK_SPEED="${2:-}";         shift 2 ;;
    -h|--help)       usage; exit 0 ;;
    *)               err "unknown arg: $1"; usage; exit 1 ;;
  esac
done

# ---- Validate ----
[[ -n "${TOOL}" ]]     || { err "--tool is required"; usage; exit 1; }
[[ -n "${ELF_FILE}" ]] || { err "--elf is required";  usage; exit 1; }
[[ -f "${ELF_FILE}" ]] || { err "file not found: ${ELF_FILE}"; exit 1; }

case "${TOOL}" in
  openocd) run_openocd ;;
  jlink)   run_jlink ;;
  *)       err "unsupported --tool: ${TOOL}"; usage; exit 1 ;;
esac
