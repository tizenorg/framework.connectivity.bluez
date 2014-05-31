%define _optdir /opt

Name:       bluetooth-share
Summary:    Bluetooth file share Agent
Version:    0.0.47
Release:    2
Group:      TO_BE/FILLED_IN
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Requires(post): vconf
Requires(post): coreutils
Requires(post): sqlite
Requires(post): smack-utils
Requires(post): sys-assert
BuildRequires:  cmake
BuildRequires:  gettext-tools
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(bluetooth-api)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(calendar-service2)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(syspopup-caller)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(libprivilege-control)
BuildRequires:  pkgconfig(capi-content-media-content)

%description
Bluetooth File Share Agent

%package -n libbluetooth-share
Summary:    Bluetooth share library
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description -n libbluetooth-share
Bluetooth share library

%package -n libbluetooth-share-devel
Summary:    Development package for libbluetooth-share
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description -n libbluetooth-share-devel
Development package for libbluetooth-share

%prep
%setup -q

%build
export CFLAGS+=" -fpie -fvisibility=hidden"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make

%install
rm -rf %{buildroot}
%make_install
mkdir -p  %{buildroot}%{_optdir}/share/bt-ftp
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}
cp LICENSE.APLv2 %{buildroot}/usr/share/license/libbluetooth-share
cp LICENSE.APLv2 %{buildroot}/usr/share/license/libbluetooth-share-devel

%post
# For the FTP server folder
if  [ ! -e /opt/share/bt-ftp ]
then
	mkdir -p  /opt/share/bt-ftp
fi

if  [ ! -e /opt/share/bt-ftp/Media ]
then
	ln -s /opt/usr/media /opt/share/bt-ftp/Media
fi

if  [ ! -e /opt/share/bt-ftp/SD_External ]
then
	ln -s /opt/storage/sdcard /opt/share/bt-ftp/SD_External
fi

if [ ! -f /opt/usr/dbspace/.bluetooth_trasnfer.db ]
then
	sqlite3 /opt/usr/dbspace/.bluetooth_trasnfer.db 'PRAGMA journal_mode = PERSIST;
	create table if not exists inbound (
		id INTEGER PRIMARY KEY autoincrement,
		sid INTEGER,
		tr_status INTEGER,
		file_path TEXT,
		dev_name TEXT,
		timestamp INTEGER default 0,
		addr TEXT,
		type TEXT,
		content TEXT,
		size INTEGER
	);
	create table if not exists outbound (
		id INTEGER PRIMARY KEY autoincrement,
		sid INTEGER,
		tr_status INTEGER,
		file_path TEXT,
		dev_name TEXT,
		timestamp INTEGER default 0,
		addr TEXT,
		type TEXT,
		content TEXT,
		size INTEGER
	);
	'
fi

chown :5000 /opt/usr/dbspace/.bluetooth_trasnfer.db
chown :5000 /opt/usr/dbspace/.bluetooth_trasnfer.db-journal
chmod 660 /opt/usr/dbspace/.bluetooth_trasnfer.db
chmod 660 /opt/usr/dbspace/.bluetooth_trasnfer.db-journal

if [ -f /usr/lib/rpm-plugins/msm.so ]
then
chsmack -a 'bt_share::db' /opt/usr/dbspace/.bluetooth_trasnfer.db
chsmack -a 'bt_share::db' /opt/usr/dbspace/.bluetooth_trasnfer.db-journal
fi

%files
%manifest bluetooth-share.manifest
/opt/etc/smack/accesses.d/bluetooth-share.rule
%defattr(-,root,root,-)
%{_bindir}/bluetooth-share
%{_datadir}/dbus-1/services/org.bluetooth.share.service
%{_optdir}/share/bt-ftp
/usr/share/license/%{name}

%files -n libbluetooth-share
%defattr(-, root, root)
%{_libdir}/libbluetooth-share-api.so.0.*
/usr/share/license/libbluetooth-share

%files -n libbluetooth-share-devel
%defattr(-, root, root)
/usr/include/bluetooth-share-api/bluetooth-share-api.h
%{_libdir}/libbluetooth-share-api.so
%{_libdir}/pkgconfig/bluetooth-share-api.pc
/usr/share/license/libbluetooth-share-devel
