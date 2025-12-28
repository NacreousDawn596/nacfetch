pkgname=nacfetch
pkgver=0.0.1
pkgrel=1
pkgdesc="Fast modern C++ system information fetcher with ASCII logos"
arch=(x86_64)
url="https://github.com/NacreousDawn596/nacfetch"
license=(GPL3)
depends=(gcc-libs)
makedepends=(cmake)
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  cd "$pkgname-$pkgver"
  cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build build
}

package() {
  cd "$pkgname-$pkgver"

  install -Dm755 output/nacfetch-linux-generic \
    "$pkgdir/usr/bin/nacfetch"

  install -Dm644 README.md \
    "$pkgdir/usr/share/doc/$pkgname/README.md"

  install -Dm644 LICENSE \
    "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
