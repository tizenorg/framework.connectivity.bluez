
Name:       bluez
Summary:    Bluetooth utilities
Version:    4.98
Release:    1
Group:      Applications/System
License:    GPLv2+
URL:        http://www.bluez.org/
Source0:    http://www.kernel.org/pub/linux/bluetooth/%{name}-%{version}.tar.gz
Requires:   bluez-libs = %{version}
Requires:   dbus >= 0.60
Requires:   usbutils
Requires:   pciutils
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(sndfile)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  flex
BuildRequires:  bison


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



%package libs
Summary:    Libraries for use in Bluetooth applications
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description libs
Libraries for use in Bluetooth applications.

%package libs-devel
Summary:    Development libraries for Bluetooth applications
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}

%description libs-devel
bluez-libs-devel contains development libraries and headers for
use in Bluetooth applications.


%package cups
Summary:    CUPS printer backend for Bluetooth printers
Group:      System/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}
Requires:   cups

%description cups
This package contains the CUPS backend

%package alsa
Summary:    ALSA support for Bluetooth audio devices
Group:      System/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}

%description alsa
This package contains ALSA support for Bluetooth audio devices

%package gstreamer
Summary:    GStreamer support for SBC audio format
Group:      System/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}

%description gstreamer
This package contains gstreamer plugins for the Bluetooth SBC audio format

%package test
Summary:    Test Programs for BlueZ
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}
Requires:   bluez-libs = %{version}
Requires:   dbus-python
Requires:   pygobject2

%description test
Scripts for testing BlueZ and its functionality


%prep
%setup -q -n %{name}-%{version}


%build

export CFLAGS="${CFLAGS} -D__TIZEN_PATCH__ -D__BROADCOM_PATCH__"
%reconfigure --disable-static \
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
                        --enable-gstreamer \
                        --disable-dfutool \
                        --disable-cups \
                        --disable-tests \
                        --disable-udevrules \
			--enable-dbusoob \
                        --with-telephony=tizen

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig


%docs_package


%files
%defattr(-,root,root,-)
%{_bindir}/ciptool
%{_bindir}/hcitool
%{_bindir}/l2ping
%{_bindir}/rfcomm
%{_bindir}/sdptool
%{_sbindir}/*
%config(noreplace) %{_sysconfdir}/bluetooth/*
%config %{_sysconfdir}/dbus-1/system.d/bluetooth.conf
/usr/lib/udev/rules.d/97-bluetooth.rules
#%{_localstatedir}/lib/bluetooth
#/lib/udev/*


%files libs
%defattr(-,root,root,-)
%{_libdir}/libbluetooth.so.*
%doc COPYING

%files libs-devel
%defattr(-, root, root)
%{_libdir}/libbluetooth.so
%dir %{_includedir}/bluetooth
%{_includedir}/bluetooth/*
%{_libdir}/pkgconfig/bluez.pc


%files alsa
%defattr(-,root,root,-)
#%{_libdir}/alsa-lib/*.so
#%{_datadir}/alsa/bluetooth.conf

%files gstreamer
%defattr(-,root,root,-)
%{_libdir}/gstreamer-*/*.so

