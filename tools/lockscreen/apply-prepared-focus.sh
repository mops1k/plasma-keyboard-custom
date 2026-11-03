#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
# Explicit interactive installation only. Never invoked by the RPM or daemon.
set -euo pipefail
# WITHDRAWN: real KScreenLocker testing regressed the existing touch/release UI.
# Keep the reviewed installer below for investigation, never deploy this version.
echo 'Focus patch withdrawn after runtime regression. Restore the original theme; do not install this patch.' >&2
exit 1
stage=${1:?prepared directory required}
target=${2:?reviewed TOUCHFIX source directory required}
if [[ $EUID != 0 || -z ${SUDO_USER:-} || -z ${SUDO_UID:-} || $SUDO_UID == 0 ]]; then
    echo 'Run explicitly through sudo from the desktop user account.' >&2
    exit 1
fi
active=$(runuser -u "$SUDO_USER" -- env XDG_RUNTIME_DIR="/run/user/$SUDO_UID" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$SUDO_UID/bus" \
    qdbus org.freedesktop.ScreenSaver /ScreenSaver org.freedesktop.ScreenSaver.GetActive)
if [[ $active != false ]]; then
    echo 'Refusing to change the theme while locked or lock state is unknown.' >&2
    exit 1
fi
(cd "$stage" && sha256sum -c SHA256SUMS)
read -r expected < "$stage/original.sha256"
[[ $expected =~ ^[0-9a-f]{64}$ ]]
current=$(sha256sum "$target/LockScreenUi.qml" | cut -d ' ' -f1)
if [[ $current != "$expected" ]]; then
    echo 'Theme changed after preparation; inspect and prepare it again.' >&2
    exit 1
fi
if [[ -e $target/LocalTouchFocus.qml ]]; then
    echo 'LocalTouchFocus.qml already exists; refusing to overwrite it.' >&2
    exit 1
fi
backup=$(mktemp -d "$target/.touch-focus-backup.XXXXXX")
cp --preserve=all "$target/LockScreenUi.qml" "$backup/LockScreenUi.qml"
temp_main=$(mktemp "$target/.local-touch-main.XXXXXX")
temp_helper=$(mktemp "$target/.local-touch-helper.XXXXXX")
helper_created=0
applied=0
cleanup() {
    rm -f -- "$temp_main" "$temp_helper"
    if [[ $helper_created == 1 && $applied == 0 ]]; then
        rm -f -- "$target/LocalTouchFocus.qml"
    fi
}
trap cleanup EXIT
# Inherit ownership, mode and SELinux context from the reviewed theme file.
cp --preserve=all "$target/LockScreenUi.qml" "$temp_main"
cp --preserve=all "$target/LockScreenUi.qml" "$temp_helper"
cp "$stage/LockScreenUi.qml" "$temp_main"
cp "$stage/LocalTouchFocus.qml" "$temp_helper"
mv -- "$temp_helper" "$target/LocalTouchFocus.qml"
helper_created=1
mv -- "$temp_main" "$target/LockScreenUi.qml"
applied=1
printf 'Prepared focus patch installed. Backup: %s\n' "$backup"
printf 'No lock/unlock, PAM changes, process restart or input collection performed.\n'
printf 'Rollback: sudo cp --preserve=all "%s/LockScreenUi.qml" "%s/LockScreenUi.qml"\n' "$backup" "$target"
