Name: @PACKAGE@
Summary: qmail-queue filter front end
Version: @VERSION@
Release: 2
Copyright: GPL
Group: Utilities/System
Source: http://em.ca/~bruceg/@PACKAGE@/%{PACKAGE_VERSION}/@PACKAGE@-%{PACKAGE_VERSION}.tar.gz
BuildRoot: /tmp/@PACKAGE@-root
URL: http://em.ca/~bruceg/@PACKAGE@/
Packager: Bruce Guenter <bruceg@em.ca>

%description
This package allows SMTP relaying for any host that authenticates with POP3.

%prep
%setup

%build
make CFLAGS="$RPM_OPT_FLAGS" LDFLAGS="-s" prefix=/usr

%install
rm -fr $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT/usr install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING README deny-filetypes
/usr/bin/qmail-qfilter
/usr/man/man1/*.1
