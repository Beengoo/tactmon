pkgname=tactmon
pkgver=1.0
pkgrel=1
pkgdesc="Simple realtime beat detector using aubio and pipewire"
arch=('x86_64')
url="https://github.com/Beengoo/tactmon"
license=('MIT')
depends=('pipewire' 'aubio')
options=('!debug')
makedepends=('gcc' 'pkgconf')
source=("tactmon.c")
sha256sums=('SKIP')

build() {
    gcc -o tactmon tactmon.c -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 $(pkg-config --libs libpipewire-0.3 aubio)
}

package() {
  install -Dm755 tactmon "$pkgdir/usr/bin/tactmon"
}
