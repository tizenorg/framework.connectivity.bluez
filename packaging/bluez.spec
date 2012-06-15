Name:       bluez
Summary:    Bluetooth utilities
Version:    4.99
Release:    3
Group:      Applications/System
License:    GPLv2+
URL:        http://www.bluez.org/
Source0:    http://www.kernel.org/pub/linux/bluetooth/%{name}-%{version}.tar.gz
Source1001: packaging/bluez.manifest
Patch1 :    bluez-ncurses.patch
Requires:   dbus >= 0.60
Requires:   usbutils
Requires:   pciutils
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(sndfile)
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
cp %{SOURCE1001} .

export CFLAGS="${CFLAGS} -D__TIZEN_PATCH__ -D__BROADCOM_PATCH__ "
export LDFLAGS=" -lncurses -Wl,--as-needed "
%reconfigure --disable-static \
			--sysconfdir=%{_sysconfdir} \
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
                        --disable-tests \
			--enable-health \
                        --disable-udevrules \
			--with-telephony=tizen

make

%install
rm -rf %{buildroot}
%make_install

install -D -m 0644 audio/audio.conf %{buildroot}%{_sysconfdir}/bluetooth/audio.conf
install -D -m 0644 network/network.conf %{buildroot}%{_sysconfdir}/bluetooth/network.conf


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
%{_bindir}/ciptool
%{_bindir}/l2ping
%{_bindir}/sdptool
%{_bindir}/gatttool
%{_bindir}/rfcomm
%{_bindir}/hcitool
%dir %{_libdir}/bluetooth/plugins
%dir /opt/var/lib/bluetooth
%exclude /lib/udev/rules.d/97-bluetooth.rules


%files -n libbluetooth3
%manifest bluez.manifest
%defattr(-,root,root,-)
%{_libdir}/libbluetooth.so.*


%files -n libbluetooth-devel
%manifest bluez.manifest
%defattr(-, root, root)
%{_includedir}/bluetooth/*
%{_libdir}/libbluetooth.so
%{_libdir}/pkgconfig/bluez.pc
