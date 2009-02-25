#!/bin/sh
# Script to install everything needed to build chromium (well, ideally, anyway)
# See http://code.google.com/p/chromium/wiki/LinuxBuildInstructions
# and http://code.google.com/p/chromium/wiki/LinuxBuild64Bit
set -ex

# Root can't access files on all filesystems, but /tmp should always be ok
# (unless it's full).
DIR=`mktemp -d`
cd $DIR
touch .created

cleanup() {
  test -f $DIR/.created && rm -rf $DIR
}

# TODO(dkegel): add sha1sum verification
download() {
  dir=$1
  file=$2
  if ! test -f $file
  then
    wget $MIRROR/$dir/$file
  fi
}

unpack_deb() {
  file=$1
  ar x $file
  tar -xzvf data.tar.gz
  rm -f data.tar.gz control.tar.gz
}

download_deb() {
  download $1 $2
  unpack_deb $2
}

install_hardy() {
  sudo apt-get install subversion pkg-config python perl g++ g++-multilib \
       bison flex gperf libnss3-dev libglib2.0-dev libgtk2.0-dev \
       libnspr4-0d libnspr4-dev wdiff lighttpd php5-cgi msttcorefonts \
       sun-java6-fonts
}

install_hardy_64() {
  install_hardy

  # The packages libnspr4, libnss3, and libsqlite don't have 32
  # bit compabibility versions on 64 bit ubuntu hardy,
  # so install them packages the hard way
  # See https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/246911
  # TODO: There is no bug report yet for 32 bit sqlite runtime

  mkdir -p workdir-hardy64
  cd workdir-hardy64
  rm -rf usr

  MIRROR=http://mirrors.kernel.org/ubuntu
  download_deb pool/main/n/nspr libnspr4-0d_4.7.1~beta2-0ubuntu1_i386.deb
  download_deb pool/main/n/nss libnss3-1d_3.12.0~beta3-0ubuntu1_i386.deb
  download_deb pool/main/s/sqlite3 libsqlite3-0_3.4.2-2_i386.deb

  sudo rsync -v -a usr/lib/* /usr/lib32/
  sudo ldconfig

  # Make missing symlinks as described by
  # https://bugs.launchpad.net/ubuntu/+source/ia32-libs/+bug/277772
  # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=492453
  # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=497087
  cd /usr/lib32
  for lib in gio-2.0 gdk-x11-2.0 atk-1.0 gdk_pixbuf-2.0 \
           pangocairo-1.0 pango-1.0 pangoft2-1.0 \
           gobject-2.0 gmodule-2.0 glib-2.0 gtk-x11-2.0; do
    sudo ln -s -f lib$lib.so.0 lib$lib.so
  done
  for lib in z fontconfig
  do
    sudo ln -s -f lib$lib.so.1 lib$lib.so
  done
  for lib in cairo
  do
    sudo ln -s -f lib$lib.so.2 lib$lib.so
  done
  for lib in freetype
  do
    sudo ln -s -f lib$lib.so.6 lib$lib.so
  done
  for lib in plds4 plc4 nspr4
  do
    sudo ln -s -f /usr/lib32/lib$lib.so.0d /usr/lib32/lib$lib.so
  done

  for lib in nss3 nssutil3 smime3 ssl3
  do
    sudo ln -s -f /usr/lib32/lib$lib.so.1d /usr/lib32/lib$lib.so
  done

  sudo ln -s -f /usr/lib32/libX11.so.6 /usr/lib32/libX11.so
  sudo ln -s -f /usr/lib32/libXrender.so.1 /usr/lib32/libXrender.so
  sudo ln -s -f /usr/lib32/libXext.so.6 /usr/lib32/libXext.so
}

if egrep -q "Ubuntu 8.04|Ubuntu 8.10" /etc/issue && test `uname -m` = i686
then
  install_hardy
elif egrep -q "Ubuntu 8.04|Ubuntu 8.10" /etc/issue && test `uname -m` = x86_64
then
  install_hardy_64
else
  echo "Unsupported system"
  cleanup
  exit 1
fi
cleanup

