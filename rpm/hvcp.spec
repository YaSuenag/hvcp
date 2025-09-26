Name: hvcp-server
Version: 0.1.1
Release: 1%{?dist}
Summary: hvcp daemon to copy file between Hyper-V host and Linux guests
License: Apache-2.0
Vendor: Y's Factory
Source: hvcp.tar.gz

# Requires for building
BuildRequires: gcc-c++
BuildRequires: make

%description
Linux daemon for hvcp - file copy tool between Hyper-V host and Linux guests.
Copyright 2025 Yasumasa Suenaga

%prep
%setup -q -n hvcp

%build
make -C linux

%install
mkdir -p %{buildroot}/%{_bindir}
mkdir -p %{buildroot}/%{_unitdir}
make -C linux install BINDIR=%{_bindir} PREFIX=%{buildroot}

%preun
if [ $1 -eq 0 ]; then
  systemctl stop hvcp || true
fi

%files
%{_bindir}/hvcp-server
%{_unitdir}/hvcp.service

%changelog
* Fri Sep 26 2025 Yasumasa Suenaga <yasuenag@gmail.com>
- Add SPEC for hvcp-server
