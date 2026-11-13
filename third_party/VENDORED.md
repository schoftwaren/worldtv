mpvqt vendored from https://invent.kde.org/libraries/mpvqt (master, ~1.2.0+, 2026-09-19).
Replaced upstream CMakeLists.txt with ECM-free shim (CMakeLists.txt) because
extra-cmake-modules is not installed and sudo needs a password tonight.
Only src/ is used; shim builds a static MpvQt::MpvQt from src/*.cpp.
