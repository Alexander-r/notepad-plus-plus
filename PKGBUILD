pkgname=notepad-plus-plus-qt
pkgver=8.9.8.1.r7226.78fd41711
pkgrel=1
pkgdesc="Source code editor and Notepad replacement (Qt port of Notepad++)"
license=('GPL-3.0-or-later')
url="https://github.com/Alexander-r/notepad-plus-plus.git"
arch=('x86_64')
depends=('qt6-base' 'qt6-5compat' 'hunspell' 'hicolor-icon-theme')
makedepends=('cmake' 'git')
source=('git+https://github.com/Alexander-r/notepad-plus-plus.git')
sha256sums=('SKIP')

pkgver() {
  cd notepad-plus-plus
  local version
  version=$(sed -nE 's/^project\(NotepadPlusPlus VERSION ([^ )]+).*/\1/p' CMakeLists.txt)
  printf "%s.r%s.%s" "$version" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cmake -S notepad-plus-plus -B build \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=None \
    -DNPP_BUILD_TESTS=OFF
  cmake --build build
}

package() {
  DESTDIR="$pkgdir" cmake --install build
  install -Dm644 notepad-plus-plus/LICENSE -t "$pkgdir/usr/share/licenses/$pkgname/"
}
