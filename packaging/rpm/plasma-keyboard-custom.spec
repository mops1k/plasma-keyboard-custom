# SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Fedora/Bazzite package for the custom plasma-keyboard fork. Built from the
# source tarball that packaging/rpm/build.sh creates from HEAD, so the package
# always matches a committed tree.
#
# The version is written by packaging/rpm/build.sh from PROJECT_VERSION in the
# top-level CMakeLists.txt; the release is 1%{?dist} (the container's dist tag is
# .fc43/.fc44), so one spec serves every Fedora branch.
#
# Unlike Arch, Fedora does not package the CMake package Qt6WaylandClientPrivate
# (the private headers themselves are in qt6-qtbase-private-devel, but no
# Qt6WaylandClientPrivateConfig.cmake). The project requires it, so %build
# generates that config around the packaged headers instead of patching the
# project.

Name:           plasma-keyboard-custom
Version:        6.7.90
Release:        1%{?dist}
Summary:        Virtual keyboard for Qt based desktops (custom fork with gamepad support)

License:        GPL-2.0-only OR GPL-3.0-only
URL:            https://github.com/mops1k/plasma-keyboard-custom
Source0:        %{name}-%{version}.tar.gz
# The speech recognition engines (Whisper and Parakeet) are built from
# whisper.cpp. Its sources come in as a second source instead of being fetched
# during the build, so the build does not need the network.
%global whisper_version 1.9.3
Source1:        whisper.cpp-%{whisper_version}.tar.gz

BuildArch:      x86_64

BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  extra-cmake-modules
BuildRequires:  gettext
BuildRequires:  pkgconf
BuildRequires:  desktop-file-utils
BuildRequires:  kf6-rpm-macros
BuildRequires:  wayland-devel
BuildRequires:  wayland-protocols-devel
BuildRequires:  libxkbcommon-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtvirtualkeyboard-devel
BuildRequires:  qt6-qtmultimedia-devel
BuildRequires:  qt6-qtwayland-devel
BuildRequires:  layer-shell-qt-devel
BuildRequires:  libplasma-devel
BuildRequires:  plasma-wayland-protocols-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kcrash-devel
BuildRequires:  kf6-kglobalaccel-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kirigami-devel
BuildRequires:  kf6-kirigami-addons-devel
BuildRequires:  kf6-kitemmodels-devel

# The shared libraries are picked up by RPM's automatic dependency generator;
# only the QML modules and the data the application loads at runtime have to be
# named here.
Requires:       qt6-qtbase
Requires:       qt6-qtdeclarative
Requires:       qt6-qtvirtualkeyboard
Requires:       qt6-qtmultimedia
Requires:       qt6-qtwayland
Requires:       layer-shell-qt
Requires:       libplasma
Requires:       kf6-kcmutils
Requires:       kf6-kconfig
Requires:       kf6-kcoreaddons
Requires:       kf6-kcrash
Requires:       kf6-kglobalaccel
Requires:       kf6-ki18n
Requires:       kf6-kirigami
Requires:       kf6-kirigami-addons
Requires:       kf6-kitemmodels
Requires:       libxkbcommon

# libhunspell is dlopen()ed at runtime: with it (and a dictionary) the spell
# checking and the corrections use the system dictionaries, without it
# everything works on the word lists compiled into the package.
Recommends:     hunspell

# This package only installs custom-named files, so it can be installed next to
# the official plasma-keyboard package. The KCM is a subpackage because it is
# only useful where plasma-systemsettings is installed.
%description
A fork of the KDE Plasma virtual keyboard with gamepad support, a floating
keyboard mode and extra themes. It is based on Qt Virtual Keyboard and talks to
the compositor over the input-method-v1 Wayland protocol.

%package -n kcm-%{name}
Summary:        %{summary}
Requires:       %{name} = %{version}-%{release}

%description -n kcm-%{name}
Configuration module for %{name}, shown in the system settings of Plasma.

%prep
%setup -q
tar -xzf %{SOURCE1} -C %{_builddir}

%build
# Fedora ships the QtWaylandClient private headers in qt6-qtbase-private-devel
# but no CMake package for them. Build the config the project's
# find_package(Qt6WaylandClientPrivate REQUIRED NO_MODULE) looks for, pointing at
# those headers, instead of patching CMakeLists.txt.
mkdir -p %{_builddir}/qtwaylandclientprivate
cat > %{_builddir}/qtwaylandclientprivate/Qt6WaylandClientPrivateConfig.cmake <<'EOF'
file(GLOB _qt_wlc_private_dirs "/usr/include/qt6/QtWaylandClient/*/QtWaylandClient/private")
if(NOT _qt_wlc_private_dirs)
    message(FATAL_ERROR "QtWaylandClient private headers not found; qt6-qtbase-private-devel is missing")
endif()
list(SORT _qt_wlc_private_dirs)
list(GET _qt_wlc_private_dirs -1 _qt_wlc_private_dir)
# The include root is the directory above <include root>/QtWaylandClient/private.
get_filename_component(_qt_wlc_include_root "${_qt_wlc_private_dir}/../.." ABSOLUTE)
if(NOT TARGET Qt6::WaylandClientPrivate)
    add_library(Qt6::WaylandClientPrivate INTERFACE IMPORTED)
    set_target_properties(Qt6::WaylandClientPrivate PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_qt_wlc_include_root}"
        INTERFACE_LINK_LIBRARIES Qt6::WaylandClient)
endif()
EOF

%cmake_kf6 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DWHISPER_CPP_SOURCE_DIR=%{_builddir}/whisper.cpp-%{whisper_version} \
  -DQt6WaylandClientPrivate_DIR=%{_builddir}/qtwaylandclientprivate
%cmake_build

%install
%cmake_install
# CMake installs the udev rule into ${KDE_INSTALL_LIBDIR}/udev/rules.d, which on
# Fedora is /usr/lib64/udev/rules.d — a directory udev never reads. Move it to
# the system rules directory so the touchscreen rule actually applies.
install -d %{buildroot}%{_prefix}/lib/udev/rules.d
mv %{buildroot}%{_libdir}/udev/rules.d/70-plasma-keyboard-touchscreen.rules \
    %{buildroot}%{_prefix}/lib/udev/rules.d/
rmdir -p %{buildroot}%{_libdir}/udev/rules.d 2>/dev/null || true

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/org.kde.plasma.keyboard.custom.desktop

%files
%license LICENSES/*
%doc README.md
%{_bindir}/plasma-keyboard-custom
%{_datadir}/applications/org.kde.plasma.keyboard.custom.desktop
%{_datadir}/metainfo/org.kde.plasma.keyboard.custom.metainfo.xml
%{_datadir}/config.kcfg/plasmakeyboardcustomsettings.kcfg
%{_datadir}/plasma/keyboard-custom/
%{_datadir}/plasma/plasmoids/org.kde.plasma.keyboard.custom.toggle/
%{_libdir}/qt6/qml/QtQuick/VirtualKeyboard/Styles/PlasmaBreezeCustom/
%{_libdir}/qt6/qml/org/kde/plasma/keyboard/custom/
%{_prefix}/lib/udev/rules.d/70-plasma-keyboard-touchscreen.rules
%{_datadir}/locale/*/LC_MESSAGES/kcm_plasmakeyboardcustom.mo
%{_datadir}/locale/*/LC_MESSAGES/plasma-keyboard-custom.mo

%files -n kcm-%{name}
%{_libdir}/qt6/plugins/plasma/kcms/systemsettings/kcm_plasmakeyboardcustom.so
%{_datadir}/applications/kcm_plasmakeyboardcustom.desktop

%changelog
* Tue Sep 22 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com> - 6.7.90-1
- Initial Fedora package of the custom fork
