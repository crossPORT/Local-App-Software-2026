# Building RocketBox

## Linux (Ubuntu/Debian)

```bash
sudo apt install cmake g++ pkg-config libusb-1.0-0-dev libwxgtk3.2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target rocketbox -j
./build/apps/wx/RocketBox --config booth-port0.conf
```

### Package locally (deb)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --target rocketbox
cmake --install build --prefix build/install
cd build && cpack -G DEB
```

## macOS

```bash
brew install cmake wxwidgets libusb pkg-config
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DwxWidgets_CONFIG="$(brew --prefix)/bin/wx-config"
cmake --build build --target rocketbox -j
open build/apps/wx/RocketBox.app
```

### Package locally (dmg)

```bash
cmake --install build --prefix build/install
cd build && cpack -G DragNDrop
```

## Windows

Requires [vcpkg](https://vcpkg.io):

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release --target rocketbox
```

### Package locally (NSIS)

Install [NSIS](https://nsis.sourceforge.io/), then:

```powershell
cmake --install build --prefix build/install
cd build
cpack -G NSIS -C Release
```

## Web PWA

```bash
cd apps/web
npm ci
npm test
npm run dev                    # local dev at /
./scripts/package-pwa.sh       # RocketBox-pwa.zip at repo root
```

## Tests (no wx required)

```bash
cmake -S . -B build -DBUILD_WX_GUI=OFF
cmake --build build -j
ctest --test-dir build -L unit
ctest --test-dir build -L integration
cd apps/web && npm test
```
