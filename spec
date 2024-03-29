Name: @PACKAGE@
Summary: qmail-queue filter front end
Version: @VERSION@
Release: 1
License: Unlicense
Group: Utilities/System
Source: http://untroubled.org/@PACKAGE@/@PACKAGE@-@VERSION@.tar.gz
BuildRoot: %{_tmppath}/@PACKAGE@-root
URL: http://untroubled.org/@PACKAGE@/
Packager: Bruce Guenter <bruceg@em.ca>
BuildRequires: bglibs >= 1.017

%description
This program allows the body of a message to be filtered through a
series of filters before being passed to the real qmail-queue program,
and injected into the qmail queue.

%prep
%setup

%build
echo %{_bindir} >conf-bin
echo %{_mandir} >conf-man
echo gcc %{optflags} >conf-cc
echo gcc -s >conf-ld
make

%install
rm -fr %{buildroot}
make install install_prefix=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc COPYING NEWS README samples
%{_bindir}/qmail-qfilter
%{_mandir}/man1/*
