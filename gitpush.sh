#!/usr/bin/env bash
set -euo pipefail

# Script konumuna (repo köküne) geç
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Git repo kontrolü
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "Bu dizin bir git deposu değil."
  exit 1
fi

# Mevcut branch ve commit mesajı
current_branch="$(git rev-parse --abbrev-ref HEAD)"
commit_message="${1:-chore: auto-push $(date +'%Y-%m-%d %H:%M:%S')}"

# Değişiklik var mı?
if [[ -n "$(git status --porcelain)" ]]; then
  git add -A
  git commit -m "$commit_message" || true
  # Olası uzak değişiklikleri al (çatışma yoksa) ve push et
  git pull --rebase origin "$current_branch" || true
  git push origin "$current_branch"
  echo "Gönderildi: $current_branch"
else
  echo "Commitlenecek değişiklik yok. Push atlanıyor."
fi


