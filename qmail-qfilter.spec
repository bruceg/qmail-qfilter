Name: qmail-qfilter
Summary: qmail-queue filter front end
Version: 1.3
Release: 2
Copyright: GPL
Group: Utilities/System
Source: http://em.ca/~bruceg/qmail-qfilter/%{PACKAGE_VERSION}/qmail-qfilter-%{PACKAGE_VERSION}.tar.gz
BuildRoot: /tmp/qmail-qfilter
URL: http://em.ca/~bruceg/qmail-qfilter/
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
