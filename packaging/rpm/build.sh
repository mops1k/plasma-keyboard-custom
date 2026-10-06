#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build the Fedora/Bazzite RPM from the current git tree. Run it inside a Fedora
# container with the build dependencies installed (the release workflow does
# that); the result is copied to packaging/*.rpm, next to the Arch package.
#
# Like packaging/build.sh, the source tarball is created from HEAD, so only
# committed files are packaged and the version always matches the project.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
cd "$here"

version="$(sed -n 's/^set(PROJECT_VERSION "\(.*\)")$/\1/p' "$root/CMakeLists.txt" | head -n1)"
if [ -z "$version" ]; then
    echo "Could not determine project version" >&2
    exit 1
fi

sed -i "s/^Version:.*/Version:        $version/" plasma-keyboard-custom.spec

rm -f "plasma-keyboard-custom-$version.tar.gz"
git -C "$root" archive --format=tar.gz \
    --prefix="plasma-keyboard-custom-$version/" HEAD \
    > "plasma-keyboard-custom-$version.tar.gz"

# The speech recognition engines are built from whisper.cpp (Whisper and
# Parakeet) and transcribe.cpp (GigaAM). The RPM build step has no network, so
# their source archives are fetched here, next to the project tarball (the spec
# takes all of them from this directory).
whisper_version="$(sed -n 's/^%global whisper_version \(.*\)$/\1/p' plasma-keyboard-custom.spec | head -n1)"
whisper_archive="whisper.cpp-$whisper_version.tar.gz"
if [ ! -f "$whisper_archive" ]; then
    curl -L -o "$whisper_archive" "https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v$whisper_version.tar.gz"
fi
echo "1650f884effba487025143bd8facd2f9fb40a83b3737a732803c67a8d659d9c0  $whisper_archive" | sha256sum -c -

transcribe_commit="$(sed -n 's/^%global transcribe_commit \(.*\)$/\1/p' plasma-keyboard-custom.spec | head -n1)"
transcribe_archive="transcribe.cpp-$transcribe_commit.tar.gz"
if [ ! -f "$transcribe_archive" ]; then
    curl -L -o "$transcribe_archive" "https://github.com/handy-computer/transcribe.cpp/archive/$transcribe_commit.tar.gz"
fi
echo "c6491711a6eb3c44217118f20cdf3c9ce877d175e407c7a253a52e5c0c6a824d  $transcribe_archive" | sha256sum -c -

topdir="$(mktemp -d)"
trap 'rm -rf "$topdir"' EXIT

# debug_package is disabled the same way the Arch package disables it: the debug
# package only duplicates the symbols in every release asset.
rpmbuild -bb plasma-keyboard-custom.spec \
    --define "_topdir $topdir" \
    --define "_sourcedir $here" \
    --define "debug_package %{nil}"

# The packages land next to the Arch one (packaging/), so the release workflow
# picks both up with the same glob it uses for the other artefacts.
rm -f "$here/.."/*.rpm
find "$topdir/RPMS" -name '*.rpm' -exec cp -v {} "$here/.." \;
ls -l "$here/.."/*.rpm
