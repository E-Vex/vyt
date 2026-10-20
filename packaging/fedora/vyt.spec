Name:           vyt
Version:        2.0.0
Release:        1%{?dist}
Summary:        tiny C wrapper around yt-dlp and mpv
License:        MIT
URL:            https://github.com/E-Vex/vyt
Source0:        https://github.com/E-Vex/vyt/archive/refs/tags/v%{version}.tar.gz
BuildRequires:  gcc make
Requires:       yt-dlp mpv

%description
vyt is a tiny C wrapper around yt-dlp and mpv that makes searching,
playing and organising YouTube media fast & simple from the terminal.

%prep
%autosetup

%build
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_bindir} %{buildroot}%{_mandir}/man1
install -m 0755 vyt %{buildroot}%{_bindir}/vyt
install -m 0644 man/vyt.1 %{buildroot}%{_mandir}/man1/vyt.1

%files
%{_bindir}/vyt
%{_mandir}/man1/vyt.1*

%changelog
* Wed Oct 21 2026 E-Vex <contact@e-vex.dev> - 2.0.0-1
- Initial package: add download mode (vyt -d URL)
