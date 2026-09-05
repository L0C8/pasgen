#!/usr/bin/env bash
#
# Pasgen — updater for Linux and macOS.
#
# Pulls the latest source, then hands off to install.sh, which re-checks every
# dependency and installs anything the new revision needs before rebuilding.
# Because the hand-off happens *after* the pull, the freshly pulled install.sh
# is the one that runs — so a new dependency added upstream is picked up on the
# same run. (A change to update.sh itself only takes effect the run after.)
#
# Usage:
#   ./update.sh                     Pull, then rebuild and reinstall
#   ./update.sh --check             Report whether an update is available, change nothing
#   ./update.sh --force             Rebuild even when already up to date
#   ./update.sh --prefix ~/.local   Any other flag is forwarded to install.sh
#
set -eo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_DIR"

if [ -t 1 ]; then
    BOLD=$'\033[1m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'; RED=$'\033[31m'; DIM=$'\033[2m'; RESET=$'\033[0m'
else
    BOLD=""; GREEN=""; YELLOW=""; RED=""; DIM=""; RESET=""
fi
say()  { printf '%s==>%s %s\n' "$BOLD" "$RESET" "$*"; }
ok()   { printf '    %s✓%s %s\n' "$GREEN" "$RESET" "$*"; }
note() { printf '    %s%s%s\n' "$DIM" "$*" "$RESET"; }
warn() { printf '%swarning:%s %s\n' "$YELLOW" "$RESET" "$*" >&2; }
die()  { printf '%serror:%s %s\n' "$RED" "$RESET" "$*" >&2; exit 1; }

CHECK_ONLY=0
FORCE=0
PASSTHRU=""
while [ $# -gt 0 ]; do
    case "$1" in
        --check) CHECK_ONLY=1; shift ;;
        --force) FORCE=1; shift ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) PASSTHRU="$PASSTHRU $1"; shift ;;
    esac
done

printf '%s\n' "${BOLD}=== Pasgen updater ===${RESET}"

command -v git >/dev/null 2>&1 || die "git is not installed"
git rev-parse --git-dir >/dev/null 2>&1 || die "$REPO_DIR is not a git checkout — re-clone the repo to use the updater"

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[ "$BRANCH" = "HEAD" ] && die "the checkout is in a detached HEAD state — 'git switch main' first"

if ! git rev-parse --abbrev-ref --symbolic-full-name '@{u}' >/dev/null 2>&1; then
    die "branch '$BRANCH' has no upstream remote — nothing to update from"
fi

say "Fetching origin"
git fetch --quiet --prune

LOCAL="$(git rev-parse HEAD)"
REMOTE="$(git rev-parse '@{u}')"
BASE="$(git merge-base HEAD '@{u}')"

if [ "$LOCAL" = "$REMOTE" ]; then
    ok "Already at the latest revision ($(git rev-parse --short HEAD), branch $BRANCH)"
    if [ "$CHECK_ONLY" -eq 1 ]; then exit 0; fi
    if [ "$FORCE" -eq 0 ]; then
        note "Nothing to pull. Use --force to rebuild and reinstall anyway."
        exit 0
    fi
    say "Rebuilding anyway (--force)"
else
    if [ "$LOCAL" != "$BASE" ]; then
        die "local commits on '$BRANCH' diverge from the remote — resolve that by hand (git rebase / git reset)"
    fi
    behind="$(git rev-list --count HEAD..'@{u}')"
    say "$behind new commit(s) available on $BRANCH"
    git --no-pager log --oneline --no-decorate HEAD..'@{u}' | sed 's/^/    /'

    if [ "$CHECK_ONLY" -eq 1 ]; then
        note "Run ./update.sh to apply."
        exit 0
    fi

    if ! git diff-index --quiet HEAD -- 2>/dev/null; then
        warn "You have uncommitted changes; the pull is fast-forward only and will refuse to clobber them."
    fi

    say "Pulling"
    git pull --ff-only || die "pull failed — commit or stash your local changes and retry"
    ok "Now at $(git rev-parse --short HEAD)"
fi

# install.sh re-runs the full dependency check, so a dependency introduced by
# the commits just pulled is detected and installed here.
say "Re-checking dependencies and rebuilding"
[ -x "$REPO_DIR/install.sh" ] || chmod +x "$REPO_DIR/install.sh" 2>/dev/null || true
# shellcheck disable=SC2086
exec "$REPO_DIR/install.sh" $PASSTHRU
