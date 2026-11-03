#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
set -euo pipefail
switcher=${1:?switcher path required}
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
export TEST_ROOT=$root
export PATH="$root:$PATH"
cat > "$root/kreadconfig6" <<'EOF'
#!/usr/bin/env bash
if [[ " $* " == *" --key VirtualKeyboardEnabled "* ]]; then
    printf '%s\n' "$TEST_ENABLED"
else
    head -n 1 "$TEST_ROOT/current"
fi
EOF
cat > "$root/kwriteconfig6" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "${@: -1}" > "$TEST_ROOT/current"
printf '%s\n' "${@: -1}" >> "$TEST_ROOT/writes"
EOF
cat > "$root/gdbus" <<'EOF'
#!/usr/bin/env bash
if [[ $1 == call ]]; then
    printf '(false,)\n'
else
    printf 'org.freedesktop.ScreenSaver.ActiveChanged (true,)\n'
    printf 'org.freedesktop.ScreenSaver.ActiveChanged (false,)\n'
fi
EOF
chmod +x "$root/kreadconfig6" "$root/kwriteconfig6" "$root/gdbus"

check_unchanged() {
    export TEST_ENABLED=$1
    printf '%s\n' "$2" > "$root/current"
    : > "$root/writes"
    bash "$switcher" custom.desktop stock.desktop
    test ! -s "$root/writes"
    test "$(head -n 1 "$root/current")" = "$2"
}
check_unchanged false custom.desktop
check_unchanged true ''
check_unchanged true other.desktop

export TEST_ENABLED=true
printf 'custom.desktop\n' > "$root/current"
: > "$root/writes"
bash "$switcher" custom.desktop stock.desktop
printf 'stock.desktop\ncustom.desktop\n' > "$root/expected"
cmp "$root/expected" "$root/writes"
printf 'Lock switcher: disabled/None/other preserved; managed lock/unlock works\n'
