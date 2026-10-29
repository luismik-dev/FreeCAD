#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later

# Merges the upstream branch into a sync branch based on the fork's base branch
# and pushes it, so it can be tested and proposed as a pull request.
#
# Run from within a clone of the fork whose "origin" can be pushed to.
#
# Environment:
#   UPSTREAM_REPO    upstream GitHub repository (default: FreeCAD/FreeCAD)
#   UPSTREAM_BRANCH  upstream branch to merge (default: main)
#   BASE_BRANCH      fork branch the sync is based on (default: main)
#   SYNC_BRANCH      branch the merge is pushed to (default: sync/upstream)
#   DRY_RUN          if "true", do not push
#
# Writes to $GITHUB_OUTPUT (if set):
#   status        up-to-date | unchanged | merged | manual | conflict
#   sha           commit to test (merged, manual)
#   upstream_sha  upstream commit that was merged
#   commit_count  number of upstream commits not yet in the base branch
#   merge_base    last upstream commit the base branch contains

set -euo pipefail

UPSTREAM_REPO=${UPSTREAM_REPO:-FreeCAD/FreeCAD}
UPSTREAM_BRANCH=${UPSTREAM_BRANCH:-main}
BASE_BRANCH=${BASE_BRANCH:-main}
SYNC_BRANCH=${SYNC_BRANCH:-sync/upstream}
UPSTREAM_URL=${UPSTREAM_URL:-https://github.com/${UPSTREAM_REPO}.git}
BOT_NAME='github-actions[bot]'
BOT_EMAIL='41898282+github-actions[bot]@users.noreply.github.com'

output() {
    echo "$1=$2"
    if [ -n "${GITHUB_OUTPUT:-}" ]; then
        echo "$1=$2" >> "$GITHUB_OUTPUT"
    fi
}

summary() {
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
        echo "$1" >> "$GITHUB_STEP_SUMMARY"
    else
        echo "$1"
    fi
}

git config user.name "$BOT_NAME"
git config user.email "$BOT_EMAIL"

if git remote get-url upstream > /dev/null 2>&1; then
    git remote set-url upstream "$UPSTREAM_URL"
else
    git remote add upstream "$UPSTREAM_URL"
fi

git fetch --no-tags upstream "+refs/heads/${UPSTREAM_BRANCH}:refs/remotes/upstream/${UPSTREAM_BRANCH}"
git fetch --no-tags origin "+refs/heads/${BASE_BRANCH}:refs/remotes/origin/${BASE_BRANCH}"
sync_sha=
if git fetch --no-tags origin "+refs/heads/${SYNC_BRANCH}:refs/remotes/origin/${SYNC_BRANCH}" 2> /dev/null; then
    sync_sha=$(git rev-parse "origin/${SYNC_BRANCH}")
fi

upstream_sha=$(git rev-parse "upstream/${UPSTREAM_BRANCH}")
base_sha=$(git rev-parse "origin/${BASE_BRANCH}")
output upstream_sha "$upstream_sha"
output commit_count "$(git rev-list --count "${base_sha}..${upstream_sha}")"
output merge_base "$(git merge-base "$base_sha" "$upstream_sha")"

if git merge-base --is-ancestor "$upstream_sha" "$base_sha"; then
    summary "${BASE_BRANCH} already contains ${UPSTREAM_REPO}@${upstream_sha:0:10}."
    output status up-to-date
    exit 0
fi

if [ -n "$sync_sha" ]; then
    if [ "$(git log -1 --format=%ce "$sync_sha")" != "$BOT_EMAIL" ] \
        && git merge-base --is-ancestor "$base_sha" "$sync_sha"; then
        # Someone pushed to the sync branch, e.g. to resolve a conflict. Keep it.
        summary "${SYNC_BRANCH} has manual changes, testing it as is."
        output status manual
        output sha "$sync_sha"
        exit 0
    fi
    if git merge-base --is-ancestor "$upstream_sha" "$sync_sha" \
        && git merge-base --is-ancestor "$base_sha" "$sync_sha"; then
        summary "${SYNC_BRANCH} already merges ${UPSTREAM_REPO}@${upstream_sha:0:10} into ${BASE_BRANCH}."
        output status unchanged
        exit 0
    fi
fi

git switch --force-create "$SYNC_BRANCH" "$base_sha"
if ! git merge --no-ff --no-edit \
    -m "Merge ${UPSTREAM_REPO} ${UPSTREAM_BRANCH} (${upstream_sha:0:10}) into ${BASE_BRANCH}" \
    "$upstream_sha"; then
    conflicts=$(git diff --name-only --diff-filter=U)
    git merge --abort
    summary "## Merging ${UPSTREAM_REPO}@${upstream_sha:0:10} into ${BASE_BRANCH} conflicts"
    summary ""
    summary '```'
    summary "$conflicts"
    summary '```'
    summary ""
    summary "Resolve it locally and push the result, which tests it and opens the pull request:"
    summary ""
    summary '```sh'
    summary "git fetch origin ${BASE_BRANCH}"
    summary "git switch --force-create ${SYNC_BRANCH} origin/${BASE_BRANCH}"
    summary "git pull --no-ff ${UPSTREAM_URL} ${UPSTREAM_BRANCH}"
    summary "# resolve the conflicts, then"
    summary "git commit"
    summary "git push --force origin ${SYNC_BRANCH}"
    summary '```'
    output status conflict
    exit 0
fi

merge_sha=$(git rev-parse HEAD)
if [ "${DRY_RUN:-false}" != "true" ]; then
    git push --force-with-lease="refs/heads/${SYNC_BRANCH}:${sync_sha}" origin "HEAD:refs/heads/${SYNC_BRANCH}"
fi
summary "Merged ${UPSTREAM_REPO}@${upstream_sha:0:10} into ${BASE_BRANCH} as ${SYNC_BRANCH}@${merge_sha:0:10}."
output status merged
output sha "$merge_sha"
