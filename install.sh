#!/bin/sh

set -e -x

echo "Target architecture is $TARGETARCH"

target=/build/install
mkdir -p $target/bin

apk update

build_install() {
  cd /build/busybox
  # Download + extract
  busybox_base=https://busybox.net/downloads/
  wget "$busybox_base/busybox-$ENVHTTPD_BUSYBOX_VERSION.tar.bz2" \
    -O /build/busybox.tar.bz2
  tar -x --strip-components=1 -f ../busybox.tar.bz2
  # Configure
  sed -i -e "s/CONFIG_PREFIX=.*/CONFIG_PREFIX=\"${target//\//\\\/}\"/" .config
  case "$TARGETARCH" in
    *64*)
      sed -i -e 's/CONFIG_LINUX64=.*/CONFIG_LINUX64=y/' .config
      sed -i -e 's/CONFIG_LINUX32=.*/CONFIG_LINUX32=n/' .config
      ;;
    *)
      sed -i -e 's/CONFIG_LINUX64=.*/CONFIG_LINUX64=n/' .config
      sed -i -e 's/CONFIG_LINUX32=.*/CONFIG_LINUX32=y/' .config
      ;;
  esac
  # Build
  apk add alpine-sdk
  make
  ./make_single_applets.sh
  make V=1 install;
}

package_install() {
  # We haven't figured out the compact build/install from source for this
  # platform... install bulkier busybox / busybox-extras from alpine packages
  # instead, which still makes for a less than 2mb image
  apk add busybox-extras
  cd $target/bin
  cp -av /bin/busybox* ./
  for exe in cat cut env mkdir rev sh tr sed; do
    ln -sv busybox $exe
  done
  ln -sv busybox-extras httpd
}

case "$TARGETARCH" in
  amd64|arm64) build_install;;
  *)           package_install;;
esac

mkdir -p $target/lib
cp -a /lib/*musl* $target/lib

apk add tini
cp /sbin/tini $target/bin/

