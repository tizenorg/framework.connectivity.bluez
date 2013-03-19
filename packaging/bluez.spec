Name:       bluez
Summary:    Bluetooth utilities
Version:    4.101_22
Release:    22
Group:      Applications/System
License:    GPLv2+
URL:        http://www.bluez.org/
Source0:    http://www.kernel.org/pub/linux/bluetooth/%{name}-%{version}.tar.gz
Patch1 :    bluez-ncurses.patch
Patch2 :    disable-eir-unittest.patch
Requires:   dbus >= 0.60
Requires:   pciutils
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  flex
BuildRequires:  bison
BuildRequires:  readline-devel

%description
Utilities for use in Bluetooth applications:
	--dfutool
	--hcitool
	--l2ping
	--rfcomm
	--sdptool
	--hciattach
	--hciconfig
	--hid2hci

The BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., U.S.A.



%package -n libbluetooth3
Summary:    Libraries for use in Bluetooth applications
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}
Requires(post): eglibc
Requires(postun): eglibc

%description -n libbluetooth3
Libraries for use in Bluetooth applications.

%package -n libbluetooth-devel
Summary:    Development libraries for Bluetooth applications
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
Requires:   libbluetooth3 = %{version}

%description -n libbluetooth-devel
bluez-libs-devel contains development libraries and headers for
use in Bluetooth applications.

%package -n bluez-test
Summary:    Test utilities for BlueZ
Group:      Test Utilities

%description -n bluez-test
bluez-test contains test utilities for BlueZ testing.

%prep
%setup -q
%patch1 -p1
%patch2 -p2


%build

export CFLAGS="${CFLAGS} -D__TIZEN_PATCH__ -D__BROADCOM_PATCH__ "
export LDFLAGS=" -lncurses -Wl,--as-needed "
%reconfigure --disable-static \
			--sysconfdir=%{_sysconfdir} \
			--localstatedir=%{_localstatedir} \
			--with-systemdunitdir=%{_libdir}/systemd/system \
                        --enable-debug \
                        --enable-pie \
                        --enable-network \
                        --enable-serial \
                        --enable-input \
                        --enable-usb=no \
                        --enable-tools \
			--disable-bccmd \
                        --enable-pcmcia=no \
                        --enable-hid2hci=no \
                        --enable-alsa=no \
                        --enable-gstreamer=no \
                        --disable-dfutool \
                        --disable-cups \
			--enable-health \
			--enable-dbusoob \
			--enable-test \
                        --with-telephony=tizen

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

install -D -m 0644 audio/audio.conf %{buildroot}%{_sysconfdir}/bluetooth/audio.conf
install -D -m 0644 network/network.conf %{buildroot}%{_sysconfdir}/bluetooth/network.conf

install -D -m 0644 COPYING %{buildroot}%{_datadir}/license/bluez
install -D -m 0644 COPYING %{buildroot}%{_datadir}/license/libbluetooth3
install -D -m 0644 COPYING %{buildroot}%{_datadir}/license/libbluetooth-devel

ln -s bluetooth.service %{buildroot}%{_libdir}/systemd/system/dbus-org.bluez.service

%post -n libbluetooth3 -p /sbin/ldconfig

%postun -n libbluetooth3 -p /sbin/ldconfig


%files
%manifest bluez.manifest
%defattr(-,root,root,-)
%{_sysconfdir}/bluetooth/audio.conf
%{_sysconfdir}/bluetooth/main.conf
%{_sysconfdir}/bluetooth/network.conf
%{_sysconfdir}/bluetooth/rfcomm.conf
%{_sysconfdir}/dbus-1/system.d/bluetooth.conf
%{_datadir}/man/*/*
%{_sbindir}/bluetoothd
%{_sbindir}/hciconfig
%{_sbindir}/hciattach
%exclude %{_bindir}/ciptool
%{_bindir}/l2ping
%{_bindir}/sdptool
%{_bindir}/gatttool
%{_bindir}/rfcomm
%{_bindir}/hcitool
%dir %{_libdir}/bluetooth/plugins
%{_libdir}/systemd/system/bluetooth.service
%{_libdir}/systemd/system/dbus-org.bluez.service
%dir %{_localstatedir}/lib/bluetooth
%{_datadir}/dbus-1/system-services/org.bluez.service
%{_datadir}/license/bluez


%files -n libbluetooth3
%defattr(-,root,root,-)
%{_libdir}/libbluetooth.so.*
%{_datadir}/license/libbluetooth3


%files -n libbluetooth-devel
%defattr(-, root, root)
%{_includedir}/bluetooth/*
%{_libdir}/libbluetooth.so
%{_libdir}/pkgconfig/bluez.pc
%{_datadir}/license/libbluetooth-devel

%files -n bluez-test
%defattr(-,root,root,-)
%{_sbindir}/hciemu
%{_bindir}/l2test
%{_bindir}/rctest
