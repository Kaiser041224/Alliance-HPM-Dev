#!/usr/bin/env bash
# Backward-compatible wrapper. New interface: `hpmdev new <name>`.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${HERE}/../bin/hpmdev" new "$@"
