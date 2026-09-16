#!/usr/bin/env bash

set -euo pipefail

project_name="${PROJECT_NAME:-unknown}"
action="${BUILD_ACTION:-unknown}"
build_status="${BUILD_STATUS:-0}"
output_dir="${OUTPUT_DIR:-}"
build_log="${BUILD_LOG:-}"
app_dir="${APP_DIR:-}"
build_dir="${BUILD_DIR:-}"
board="${BOARD:-}"
board_search_path="${BOARD_SEARCH_PATH:-}"
hpm_sdk_version="${HPM_SDK_VERSION:-}"
riscv_toolchain_version="${RISCV_TOOLCHAIN_VERSION:-}"
rv_arch="${RV_ARCH:-}"
rv_abi="${RV_ABI:-}"
cmake_build_type="${CMAKE_BUILD_TYPE:-}"
hpm_build_type="${HPM_BUILD_TYPE:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project)
      project_name="${2:-}"
      shift 2
      ;;
    --action)
      action="${2:-}"
      shift 2
      ;;
    --status)
      build_status="${2:-0}"
      shift 2
      ;;
    --output-dir)
      output_dir="${2:-}"
      shift 2
      ;;
    --log-file)
      build_log="${2:-}"
      shift 2
      ;;
    --app-dir)
      app_dir="${2:-}"
      shift 2
      ;;
    --build-dir)
      build_dir="${2:-}"
      shift 2
      ;;
    --board)
      board="${2:-}"
      shift 2
      ;;
    --board-search-path)
      board_search_path="${2:-}"
      shift 2
      ;;
    --hpm-sdk-version)
      hpm_sdk_version="${2:-}"
      shift 2
      ;;
    --riscv-toolchain-version)
      riscv_toolchain_version="${2:-}"
      shift 2
      ;;
    --rv-arch)
      rv_arch="${2:-}"
      shift 2
      ;;
    --rv-abi)
      rv_abi="${2:-}"
      shift 2
      ;;
    --cmake-build-type)
      cmake_build_type="${2:-}"
      shift 2
      ;;
    --hpm-build-type)
      hpm_build_type="${2:-}"
      shift 2
      ;;
    *)
      echo "[build_ui] Unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$hpm_sdk_version" ]]; then
  sdk_base="${HPM_SDK_BASE:-${HPMDEV_SDK_DIR:-/workspace/sdk/hpm_sdk}}"
  version_file="${sdk_base}/VERSION"
  if [[ -f "$version_file" ]]; then
    major="$(awk -F '=' '/VERSION_MAJOR/ {gsub(/[[:space:]]/, "", $2); print $2}' "$version_file")"
    minor="$(awk -F '=' '/VERSION_MINOR/ {gsub(/[[:space:]]/, "", $2); print $2}' "$version_file")"
    patch="$(awk -F '=' '/PATCHLEVEL/ {gsub(/[[:space:]]/, "", $2); print $2}' "$version_file")"
    if [[ -n "$major" && -n "$minor" && -n "$patch" ]]; then
      hpm_sdk_version="${major}.${minor}.${patch}"
    fi
  fi
  hpm_sdk_version="${hpm_sdk_version:-unknown}"
fi

if [[ -z "$riscv_toolchain_version" ]]; then
  if command -v riscv32-unknown-elf-gcc >/dev/null 2>&1; then
    riscv_toolchain_version="$(riscv32-unknown-elf-gcc --version | head -n 1)"
  elif command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
    riscv_toolchain_version="$(riscv64-unknown-elf-gcc --version | head -n 1)"
  else
    riscv_toolchain_version="unknown"
  fi
fi

ui_color=1
if [[ ! -t 1 || -n "${NO_COLOR:-}" || -n "${BUILD_UI_PLAIN:-}" ]]; then
  ui_color=0
fi
if [[ "$ui_color" == "1" ]]; then
  bar_full='█'; bar_empty='░'; ui_reset=$'\033[0m'
else
  bar_full='#'; bar_empty='-'; ui_reset=''
fi

size_to_bytes() {
  awk -v v="$1" -v u="$2" 'BEGIN {
    m = 1
    if (u == "KB" || u == "kB") m = 1024
    else if (u == "MB") m = 1048576
    else if (u == "GB") m = 1073741824
    printf "%.0f", v * m
  }'
}

format_bytes() {
  awk -v b="$1" 'BEGIN {
    split("B KB MB GB", u, " ")
    i = 1
    while (b >= 1024 && i < 4) { b /= 1024; i++ }
    if (i == 1) printf "%d %s", b, u[i]
    else printf "%.1f %s", b, u[i]
  }'
}

pct_color() {
  local p="$1"
  if [[ "$ui_color" != "1" ]]; then
    printf ''
    return
  fi
  awk -v p="$p" 'BEGIN {
    if (p >= 90) printf "\033[1;31m"
    else if (p >= 75) printf "\033[1;33m"
    else if (p >= 50) printf "\033[33m"
    else printf "\033[32m"
  }'
}

render_mem_bar() {
  local pct="$1" width="${2:-26}" filled i
  filled="$(awk -v p="$pct" -v w="$width" 'BEGIN {
    f = int(p / 100 * w + 0.5); if (f > w) f = w; if (f < 0) f = 0; print f
  }')"
  local bar=""
  for ((i = 0; i < filled; i++)); do bar+="$bar_full"; done
  for ((i = filled; i < width; i++)); do bar+="$bar_empty"; done
  printf '%s' "$bar"
}

mem_region_table() {
  local log="$1"
  [[ -n "$log" && -f "$log" ]] || return 1
  awk '
    /^Memory region/ { cap = 1; next }
    cap {
      if (NF != 6) exit
      if ($2 !~ /^[0-9]+([.][0-9]+)?$/ || $4 !~ /^[0-9]+([.][0-9]+)?$/) exit
      if ($3 !~ /^(B|KB|MB|GB)$/ || $5 !~ /^(B|KB|MB|GB)$/) exit
      if ($6 !~ /^[0-9]+([.][0-9]+)?%$/) exit
      name = $1
      sub(/:$/, "", name)
      print name, $2, $3, $4, $5, $6
      next
    }
  ' "$log"
}

printf '\n'
printf '%b\n' '\033[1;96m+---------------------------------------------------------------+\033[0m'
printf '%b\n' '\033[1;96m|  █████  ██      ██      ██  █████  ███    ██  ██████  ███████ |\033[0m'
printf '%b\n' '\033[1;96m| ██   ██ ██      ██      ██ ██   ██ ████   ██ ██       ██      |\033[0m'
printf '%b\n' '\033[1;96m| ███████ ██      ██      ██ ███████ ██ ██  ██ ██       █████   |\033[0m'
printf '%b\n' '\033[1;96m| ██   ██ ██      ██      ██ ██   ██ ██  ██ ██ ██       ██      |\033[0m'
printf '%b\n' '\033[1;96m| ██   ██ ███████ ███████ ██ ██   ██ ██   ████  ██████  ███████ |\033[0m'
printf '%b\n' '\033[1;96m|                                                               |\033[0m'
printf '%b\n' '\033[1;96m|   ██   ██ ██████  ███    ███     ██████  ███████ ██    ██     |\033[0m'
printf '%b\n' '\033[1;96m|   ██   ██ ██   ██ ████  ████     ██   ██ ██      ██    ██     |\033[0m'
printf '%b\n' '\033[1;96m|   ███████ ██████  ██ ████ ██     ██   ██ █████   ██    ██     |\033[0m'
printf '%b\n' '\033[1;96m|   ██   ██ ██      ██  ██  ██     ██   ██ ██       ██  ██      |\033[0m'
printf '%b\n' '\033[1;96m|   ██   ██ ██      ██      ██     ██████  ███████   ████       |\033[0m'
printf '%b\n' '\033[1;96m+---------------------------------------------------------------+\033[0m'
printf '\n'

printf '%b\n' '\033[1;30;47m >>> CONFIGURE SUMMARY \033[0m'
printf '%b\n' '\033[1;36m     PROJECT_NAME      :\033[0m \033[1;97m'"${project_name}"'\033[0m'
printf '%b\n' '\033[1;36m     APP_DIR           :\033[0m \033[0;97m'"${app_dir}"'\033[0m'
printf '%b\n' '\033[1;36m     BUILD_DIR         :\033[0m \033[0;97m'"${build_dir}"'\033[0m'
printf '%b\n' '\033[1;36m     BOARD             :\033[0m \033[1;93m'"${board}"'\033[0m'
printf '%b\n' '\033[1;36m     BOARD_SEARCH_PATH :\033[0m \033[0;97m'"${board_search_path}"'\033[0m'
printf '%b\n' '\033[1;36m     HPM_SDK_VERSION   :\033[0m \033[1;94m'"${hpm_sdk_version}"'\033[0m'
printf '%b\n' '\033[1;36m     RISCV_TOOLCHAIN   :\033[0m \033[1;94m'"${riscv_toolchain_version}"'\033[0m'
printf '%b\n' '\033[1;36m     RV_ARCH / RV_ABI  :\033[0m \033[1;92m'"${rv_arch} / ${rv_abi}"'\033[0m'
printf '%b\n' '\033[1;36m     BUILD_TYPE        :\033[0m \033[1;95m'"${cmake_build_type}"'\033[0m'
printf '%b\n' '\033[1;36m     HPM_BUILD_TYPE    :\033[0m \033[1;95m'"${hpm_build_type}"'\033[0m'
printf '\n'

if [[ "$build_status" == "0" ]]; then
  printf '%b\n' "\033[1;30;42m >>> BUILD SUCCESS: ${project_name} [ACTION: ${action}] \033[0m"
else
  printf '%b\n' "\033[1;37;41m >>> BUILD FAILED : ${project_name} [ACTION: ${action}] (EXIT=${build_status}) \033[0m"
  printf '%b\n' '\033[1;31m >>> ERROR DETAILS (directly below):\033[0m'
  if [[ -n "$build_log" && -f "$build_log" ]]; then
    printf '%b\n' '\033[1;33m >>> KEY ERRORS:\033[0m'
    err_lines="$(tr -d '\r' < "$build_log" | grep -iE 'error:|cmake error|failed|undefined reference|ninja: build stopped|no board named' | tail -n 20 || true)"
    if [[ -n "$err_lines" ]]; then
      printf '%s\n' "$err_lines"
    else
      printf '%b\n' '\033[1;33m >>> LAST LOG TAIL (tail -n 40):\033[0m'
      tail -n 40 "$build_log" || true
    fi
  fi
fi

if [[ -n "$output_dir" ]]; then
  if [[ "$build_status" == "0" ]]; then
    echo " >>> ARTIFACTS STATUS:"
  else
    echo " >>> ARTIFACTS STATUS (may be from previous successful build):"
  fi
  for ext in elf bin map asm; do
    artifact="${output_dir}/${project_name}.${ext}"
    if [[ -f "$artifact" ]]; then
      echo "     [OK] ${artifact}"
    else
      echo "     [--] ${artifact} (missing)"
    fi
  done
fi

mem_table=""
mem_cached=0
if mem_table="$(mem_region_table "$build_log")" && [[ -n "$mem_table" ]]; then
  if [[ -n "$build_dir" && -d "$build_dir" ]]; then
    printf '%s\n' "$mem_table" > "${build_dir}/memory_usage.txt" 2>/dev/null || true
  fi
elif [[ -n "$build_dir" && -f "${build_dir}/memory_usage.txt" ]]; then
  mem_table="$(< "${build_dir}/memory_usage.txt")"
  mem_cached=1
fi

if [[ -n "$mem_table" ]]; then
  if [[ "$mem_cached" == "1" ]]; then
    printf '%b\n' '\033[1;30;47m >>> MEMORY USAGE \033[0m \033[2m(last link, not relinked this build)\033[0m'
  else
    printf '%b\n' '\033[1;30;47m >>> MEMORY USAGE \033[0m'
  fi

  name_w=6
  while read -r r_name _; do
    [[ -n "$r_name" ]] || continue
    if ((${#r_name} > name_w)); then name_w=${#r_name}; fi
  done <<< "$mem_table"

  while read -r r_name r_used r_u1 r_size r_u2 r_pct; do
    [[ -n "$r_name" ]] || continue
    pct="${r_pct%\%}"
    used_h="$(format_bytes "$(size_to_bytes "$r_used" "$r_u1")")"
    size_h="$(format_bytes "$(size_to_bytes "$r_size" "$r_u2")")"
    color="$(pct_color "$pct")"
    bar="$(render_mem_bar "$pct" 26)"
    printf '  %s%-*s%s  %b%s%b  %b%6.2f%%%b   %9s / %-9s\n' \
      "$color" "$name_w" "$r_name" "$ui_reset" \
      "$color" "$bar" "$ui_reset" \
      "$color" "$pct" "$ui_reset" \
      "$used_h" "$size_h"
  done <<< "$mem_table"
fi

printf '\n'
