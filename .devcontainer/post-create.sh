#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   .devcontainer/post-create.sh           (default)  → opencode only
#   .devcontainer/post-create.sh --all     (manual)   → opencode + codex + claude
INSTALL_ALL=false
if [[ "${1:-}" == "--all" ]]; then
  INSTALL_ALL=true
  echo "[post-create] Running in --all mode: installing opencode + codex + claude"
fi

TARGET_USER="${TARGET_USER:-$(id -un)}"
TARGET_HOME="${TARGET_HOME:-${HOME:-$(getent passwd "$TARGET_USER" | cut -d: -f6)}}"
ZSHRC="${TARGET_HOME}/.zshrc"

ensure_dir_owned() {
  local dir="$1"
  local owner_uid="${2:-$(id -u "$TARGET_USER")}"
  local owner_gid="${3:-$(id -g "$TARGET_USER")}"

  if [ -e "$dir" ] && [ ! -w "$dir" ]; then
    chown -R "${owner_uid}:${owner_gid}" "$dir" 2>/dev/null || true
  fi

  if [ ! -e "$dir" ]; then
    mkdir -p "$dir"
  fi

  chown -R "${owner_uid}:${owner_gid}" "$dir" 2>/dev/null || true
}

link_dir_if_exists() {
  local src="$1"
  local dst="$2"
  if [ -d "$src" ]; then
    [ -e "$dst" ] && rm -rf "$dst"
    ln -snf "$src" "$dst"
    return 0
  fi
  return 1
}

copy_file_if_missing() {
  local src="$1"
  local dst="$2"
  if [ -f "$src" ] && [ ! -f "$dst" ]; then
    cp "$src" "$dst"
    return 0
  fi
  return 1
}

HOST_HOME="/host-home"
HOST_HOME_RO="/host-home-ro"
TARGET_UID="$(id -u "$TARGET_USER")"
TARGET_GID="$(id -g "$TARGET_USER")"

HOST_CONFIG_OPENCODE=""
for candidate in \
  "$HOST_HOME/.config/opencode" \
  "$HOST_HOME/AppData/Roaming/opencode"; do
  if [ -d "$candidate" ]; then
    HOST_CONFIG_OPENCODE="$candidate"
    break
  fi
done

HOST_CODEX_DIR=""
for candidate in \
  "$HOST_HOME/.codex" \
  "$HOST_HOME/.config/codex"; do
  if [ -d "$candidate" ]; then
    HOST_CODEX_DIR="$candidate"
    break
  fi
done

HOST_CLAUDE_DIR=""
for candidate in \
  "$HOST_HOME/.claude" \
  "$HOST_HOME/.config/claude"; do
  if [ -d "$candidate" ]; then
    HOST_CLAUDE_DIR="$candidate"
    break
  fi
done

HOST_AGENTS_DIR=""
for candidate in \
  "$HOST_HOME/.agents" \
  "$HOST_HOME/.config/agents"; do
  if [ -d "$candidate" ]; then
    HOST_AGENTS_DIR="$candidate"
    break
  fi
done

HOST_OPENCODE_SHARE_RO=""
for candidate in \
  "$HOST_HOME_RO/.local/share/opencode" \
  "$HOST_HOME_RO/AppData/Local/opencode"; do
  if [ -d "$candidate" ]; then
    HOST_OPENCODE_SHARE_RO="$candidate"
    break
  fi
done

ensure_dir_owned "$TARGET_HOME/.config" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.local" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.cache" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.config/opencode" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.local/share" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.local/share/opencode" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.cache/opencode" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.local/state" "$TARGET_UID" "$TARGET_GID"
ensure_dir_owned "$TARGET_HOME/.local/state/opencode" "$TARGET_UID" "$TARGET_GID"

touch "$ZSHRC"
chown "$TARGET_UID:$TARGET_GID" "$ZSHRC"
grep -qxF 'export PATH="/usr/local/bin:$PATH"' "$ZSHRC" || printf 'export PATH="/usr/local/bin:$PATH"\n' >> "$ZSHRC"
grep -qxF 'export PATH="$HOME/.local/bin:$PATH"' "$ZSHRC" || printf 'export PATH="$HOME/.local/bin:$PATH"\n' >> "$ZSHRC"

if ! command -v opencode >/dev/null 2>&1; then
  echo "[post-create] opencode not found, installing opencode-ai..."
  if command -v npm >/dev/null 2>&1; then
    npm install -g opencode-ai
  else
    echo "[post-create] ERROR: npm not found, cannot install opencode-ai." >&2
  fi
fi

if [ "$INSTALL_ALL" = "true" ]; then
  if ! command -v codex >/dev/null 2>&1; then
    echo "[post-create] codex not found, installing @openai/codex..."
    if command -v npm >/dev/null 2>&1; then
      # Clean up any stale/interrupted previous installation
      npm uninstall -g @openai/codex 2>/dev/null || true
      npm cache clean --force 2>/dev/null || true
      npm install -g @openai/codex
    else
      echo "[post-create] ERROR: npm not found, cannot install @openai/codex." >&2
    fi
  fi

  if ! command -v claude >/dev/null 2>&1; then
    echo "[post-create] claude not found, installing @anthropic-ai/claude-code..."
    if command -v npm >/dev/null 2>&1; then
      # Clean up any stale/interrupted previous installation
      npm uninstall -g @anthropic-ai/claude-code 2>/dev/null || true
      npm cache clean --force 2>/dev/null || true
      npm install -g @anthropic-ai/claude-code
    else
      echo "[post-create] ERROR: npm not found, cannot install @anthropic-ai/claude-code." >&2
    fi
  fi
fi

if command -v node >/dev/null 2>&1; then
  echo "[post-create] node version: $(node -v)"
fi

if command -v npm >/dev/null 2>&1; then
  echo "[post-create] npm version: $(npm -v)"
fi

if [ -n "$HOST_CODEX_DIR" ]; then
  link_dir_if_exists "$HOST_CODEX_DIR" "$TARGET_HOME/.codex" || true
fi

if [ -n "$HOST_CLAUDE_DIR" ]; then
  link_dir_if_exists "$HOST_CLAUDE_DIR" "$TARGET_HOME/.claude" || true
fi

if [ -n "$HOST_AGENTS_DIR" ]; then
  link_dir_if_exists "$HOST_AGENTS_DIR" "$TARGET_HOME/.agents" || true
fi

if [ -n "$HOST_CONFIG_OPENCODE" ]; then
  for f in opencode.json; do
    if [ -f "$HOST_CONFIG_OPENCODE/$f" ]; then
      ln -snf "$HOST_CONFIG_OPENCODE/$f" "$TARGET_HOME/.config/opencode/$f"
    fi
  done

  for dir in agents commands modes plugins skills tools themes; do
    if [ -d "$HOST_CONFIG_OPENCODE/$dir" ]; then
      [ -e "$TARGET_HOME/.config/opencode/$dir" ] && rm -rf "$TARGET_HOME/.config/opencode/$dir"
      ln -snf "$HOST_CONFIG_OPENCODE/$dir" "$TARGET_HOME/.config/opencode/$dir"
    fi
  done

  if [ -d "$TARGET_HOME/.config/opencode/node_modules" ]; then
    rm -rf "$TARGET_HOME/.config/opencode/node_modules"
    rm -f "$TARGET_HOME/.config/opencode/package.json" "$TARGET_HOME/.config/opencode/package-lock.json" 2>/dev/null || true
  fi

  # 插件由共享 opencode.json 的 plugin 数组声明，opencode 启动时会自动安装到本容器
  # 的缓存卷（/root/.cache/opencode），无需在此显式安装。
fi

if [ -n "$HOST_OPENCODE_SHARE_RO" ]; then
  copy_file_if_missing "$HOST_OPENCODE_SHARE_RO/auth.json" "$TARGET_HOME/.local/share/opencode/auth.json" || true
  copy_file_if_missing "$HOST_OPENCODE_SHARE_RO/mcp-auth.json" "$TARGET_HOME/.local/share/opencode/mcp-auth.json" || true
fi
