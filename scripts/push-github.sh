#!/usr/bin/env bash
#
# push-github.sh — upload this repo to GitHub.
#
# Sets (or updates) the "origin" remote and pushes. By default it pushes the
# CURRENT branch and sets it as the upstream. Flags let you push every branch,
# push tags, or point main at the current commit first.
#
# Usage:
#   scripts/push-github.sh                 # push the current branch
#   scripts/push-github.sh --all           # push ALL local branches
#   scripts/push-github.sh --all --tags    # ...and tags
#   scripts/push-github.sh --main          # fast-forward main to HEAD, then push main + current
#   scripts/push-github.sh --url git@github.com:EddyKuo/macXterm.git   # use SSH instead of HTTPS
#
# Auth note: GitHub HTTPS pushes need a Personal Access Token (your account
# password won't work). When git prompts for a password, paste a token with
# "repo" scope (https://github.com/settings/tokens). To avoid repeat prompts:
#   git config --global credential.helper osxkeychain
# Or switch to SSH with --url git@github.com:EddyKuo/macXterm.git after adding
# an SSH key to your GitHub account.

set -euo pipefail

REMOTE_NAME="origin"
REMOTE_URL="https://github.com/EddyKuo/macXterm.git"
PUSH_ALL=0
PUSH_TAGS=0
SYNC_MAIN=0

die() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }

while [ $# -gt 0 ]; do
  case "$1" in
    -a|--all)   PUSH_ALL=1 ;;
    -t|--tags)  PUSH_TAGS=1 ;;
    -m|--main)  SYNC_MAIN=1 ;;
    -u|--url)   shift; [ $# -gt 0 ] || die "--url needs a value"; REMOTE_URL="$1" ;;
    -h|--help)  sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)          die "unknown option: $1 (try --help)" ;;
  esac
  shift
done

# Run from the repo root regardless of where the script is called from.
cd "$(git rev-parse --show-toplevel)" 2>/dev/null || die "not inside a git repository"

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[ "$BRANCH" != "HEAD" ] || die "you are in a detached HEAD; check out a branch first"

# Warn (don't block) on uncommitted changes — only committed work is pushed.
if ! git diff-index --quiet HEAD -- 2>/dev/null; then
  printf '\033[33mwarning:\033[0m you have uncommitted changes; only committed work will be pushed.\n'
fi

# Point origin at the target URL (add it, or update an existing one).
if git remote get-url "$REMOTE_NAME" >/dev/null 2>&1; then
  CURRENT_URL="$(git remote get-url "$REMOTE_NAME")"
  if [ "$CURRENT_URL" != "$REMOTE_URL" ]; then
    info "updating remote '$REMOTE_NAME': $CURRENT_URL -> $REMOTE_URL"
    git remote set-url "$REMOTE_NAME" "$REMOTE_URL"
  else
    info "remote '$REMOTE_NAME' already set to $REMOTE_URL"
  fi
else
  info "adding remote '$REMOTE_NAME' -> $REMOTE_URL"
  git remote add "$REMOTE_NAME" "$REMOTE_URL"
fi

# Optionally fast-forward main to the current commit so main = latest.
if [ "$SYNC_MAIN" = 1 ] && [ "$BRANCH" != "main" ]; then
  info "fast-forwarding main to $BRANCH ($(git rev-parse --short HEAD))"
  git branch -f main HEAD
fi

info "pushing to $REMOTE_URL"
if [ "$PUSH_ALL" = 1 ]; then
  git push -u "$REMOTE_NAME" --all
else
  git push -u "$REMOTE_NAME" "$BRANCH"
  [ "$SYNC_MAIN" = 1 ] && git push -u "$REMOTE_NAME" main
fi

[ "$PUSH_TAGS" = 1 ] && { info "pushing tags"; git push "$REMOTE_NAME" --tags; }

echo
info "done. View it at: https://github.com/EddyKuo/macXterm"
