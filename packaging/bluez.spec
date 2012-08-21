Name:       bluez
Summary:    Bluetooth utilities
Version:    4.101
Release:    2
Group:      Applications/System
License:    GPLv2+
URL:        http://www.bluez.org/
Source0:    http://www.kernel.org/pub/linux/bluetooth/%{name}-%{version}.tar.gz
Patch1 :    bluez-ncurses.patch
Requires:   dbus >= 0.60
Requires:   pciutils
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  flex
BuildRequires:  bison
BuildRequires:  readline-devel

%description
Utilities for use in Bluetooth applications:
	--ciptool
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
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

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

%prep
%setup -q
%patch1 -p1


%build

export CFLAGS="${CFLAGS} -D__TIZEN_PATCH__ -D__BROADCOM_PATCH__ "
export LDFLAGS=" -lncurses -Wl,--as-needed "
%reconfigure --disable-static \
			--sysconfdir=%{_prefix}/etc \
			--localstatedir=/opt/var \
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
                        --with-telephony=tizen

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

install -D -m 0644 audio/audio.conf %{buildroot}%{_prefix}/etc/bluetooth/audio.conf
install -D -m 0644 network/network.conf %{buildroot}%{_prefix}/etc/bluetooth/network.conf


%post -n libbluetooth3 -p /sbin/ldconfig

%postun -n libbluetooth3 -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%{_prefix}/etc/bluetooth/audio.conf
%{_prefix}/etc/bluetooth/main.conf
%{_prefix}/etc/bluetooth/network.conf
%{_prefix}/etc/bluetooth/rfcomm.conf
%{_prefix}/etc/dbus-1/system.d/bluetooth.conf
%{_datadir}/man/*/*
%{_sbindir}/bluetoothd
%{_sbindir}/hciconfig
%{_sbindir}/hciattach
%{_bindir}/ciptool
%{_bindir}/l2ping
%{_bindir}/sdptool
%{_bindir}/gatttool
%{_bindir}/rfcomm
%{_bindir}/hcitool
%dir %{_libdir}/bluetooth/plugins
%dir /opt/var/lib/bluetooth
%{_datadir}/dbus-1/system-services/org.bluez.service


%files -n libbluetooth3
%defattr(-,root,root,-)
%{_libdir}/libbluetooth.so.*


%files -n libbluetooth-devel
%defattr(-, root, root)
%{_includedir}/bluetooth/*
%{_libdir}/libbluetooth.so
%{_libdir}/pkgconfig/bluez.pc
