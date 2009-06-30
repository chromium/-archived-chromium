#!/bin/bash -e

# Script to install everything needed to build chromium (well, ideally, anyway)
# See http://code.google.com/p/chromium/wiki/LinuxBuildInstructions
# and http://code.google.com/p/chromium/wiki/LinuxBuild64Bit

install_gold() {
  # Gold is optional; it's a faster replacement for ld,
  # and makes life on 2GB machines much more pleasant.

  # First make sure root can access this directory, as that's tripped up some folks.
  if sudo touch xyz.$$
  then
    sudo rm xyz.$$
  else
    echo root cannot write to the current directory, not installing gold
    return
  fi

  BINUTILS=binutils-2.19.1
  BINUTILS_URL=http://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.bz2
  BINUTILS_SHA1=88c91e36cde93433e4c4c2b2e3417777aad84526

  test -f $BINUTILS.tar.bz2 || wget $BINUTILS_URL
  if `sha1sum $BINUTILS.tar.bz2` != $BINUTILS_SHA1
  then
    echo Bad sha1sum for $BINUTILS.tar.bz2
    exit 1
  fi
  cat > binutils-fix.patch <<__EOF__
--- binutils-2.19.1/gold/reduced_debug_output.h.orig	2009-05-10 14:44:52.000000000 -0700
+++ binutils-2.19.1/gold/reduced_debug_output.h	2009-05-10 14:46:51.000000000 -0700
@@ -64,7 +64,7 @@
   void
   failed(std::string reason)
   {
-    gold_warning(reason.c_str());
+    gold_warning("%s", reason.c_str());
     failed_ = true;
   }
 
@@ -110,7 +110,7 @@
   void
   failed(std::string reason)
   {
-    gold_warning(reason.c_str());
+    gold_warning("%s", reason.c_str());
     this->failed_ = true;
   }
 
__EOF__

  tar -xjvf $BINUTILS.tar.bz2
  cd $BINUTILS
  patch -p1 < ../binutils-fix.patch
  ./configure --prefix=/usr/local/gold --enable-gold
  make -j3
  if sudo make install
  then
    # Still need to figure out graceful way of pointing gyp to use
    # /usr/local/gold/bin/ld without requiring him to set environment
    # variables.  That will go into bootstrap-linux.sh when it's ready.
    echo "Installing gold as /usr/bin/ld."
    echo "To uninstall, do 'cd /usr/bin; sudo rm ld; sudo mv ld.orig ld'"
    test -f /usr/bin/ld && sudo mv /usr/bin/ld /usr/bin/ld.orig
    sudo ln -fs /usr/local/gold/bin/ld /usr/bin/ld.gold
    sudo ln -fs /usr/bin/ld.gold /usr/bin/ld
  else
    echo "make install failed, not installing gold"
  fi
}

if ! egrep -q "Ubuntu 8.04|Ubuntu 8.10|Ubuntu 9.04" /etc/issue; then
  echo "Only Ubuntu 8.04, 8.10, and 9.04 are currently supported" >&2
  exit 1
fi

if ! uname -m | egrep -q "i686|x86_64"; then
  echo "Only x86 architectures are currently supported" >&2
  exit
fi

if [ "x$(id -u)" != x0 ]; then
  echo "Running as non-root user."
  echo "You might have to enter your password one or more times for 'sudo'."
  echo
fi

# Packages need for development
dev_list="bison fakeroot flex g++ g++-multilib gperf libasound2-dev
          libcairo2-dev libgconf2-dev libglib2.0-dev libgtk2.0-dev libnspr4-dev
          libnss3-dev libsqlite3-dev lighttpd msttcorefonts perl php5-cgi
          pkg-config python subversion wdiff"

# Full list of required run-time libraries
lib_list="libatk1.0-0 libc6 libasound2 libcairo2 libexpat1 libfontconfig1
          libfreetype6 libglib2.0-0 libgtk2.0-0 libnspr4-0d libnss3-1d
          libpango1.0-0 libpcre3 libpixman-1-0 libpng12-0 libstdc++6
          libsqlite3-0 libx11-6 libxau6 libxcb1 libxcomposite1
          libxcursor1 libxdamage1 libxdmcp6 libxext6 libxfixes3 libxi6
          libxinerama1 libxrandr2 libxrender1 zlib1g"

# Debugging symbols for all of the run-time libraries
dbg_list="libatk1.0-dbg libc6-dbg libcairo2-dbg libfontconfig1-dbg
          libglib2.0-0-dbg libgtk2.0-0-dbg libnspr4-0d-dbg libnss3-1d-dbg
          libpango1.0-0-dbg libpcre3-dbg libpixman-1-0-dbg libx11-6-dbg
          libxau6-dbg libxcb1-dbg libxcomposite1-dbg
          libxcursor1-dbg libxdamage1-dbg libxdmcp6-dbg libxext6-dbg
          libxfixes3-dbg libxi6-dbg libxinerama1-dbg libxrandr2-dbg
          libxrender1-dbg zlib1g-dbg"

# Standard 32bit compatibility libraries
cmp_list="ia32-libs lib32asound2-dev lib32readline-dev lib32stdc++6 lib32z1
          lib32z1-dev libc6-dev-i386 libc6-i386"

# Waits for the user to press 'Y' or 'N'. Either uppercase of lowercase is
# accepted. Returns 0 for 'Y' and 1 for 'N'. If an optional parameter has
# been provided to yes_no(), the function also accepts RETURN as a user input.
# The parameter specifies the exit code that should be returned in that case.
# The function will echo the user's selection followed by a newline character.
# Users can abort the function by pressing CTRL-C. This will call "exit 1".
yes_no() {
  local c
  while :; do
    c="$(trap 'stty echo -iuclc icanon 2>/dev/null' EXIT INT TERM QUIT
         stty -echo iuclc -icanon 2>/dev/null
         dd count=1 bs=1 2>/dev/null | od -An -tx1)"
    case "$c" in
      " 0a") if [ -n "$1" ]; then
               [ $1 -eq 0 ] && echo "Y" || echo "N"
               return $1
             fi
             ;;
      " 79") echo "Y"
             return 0
             ;;
      " 6e") echo "N"
             return 1
             ;;
      "")    echo "Aborted" >&2
             exit 1
             ;;
      *)     # The user pressed an unrecognized key. As we are not echoing
             # any incorrect user input, alert the user by ringing the bell.
             (tput bel) 2>/dev/null
             ;;
    esac
  done
}

echo "This script installs all tools and libraries needed to build Chromium."
echo ""
echo "For most of the libraries, it can also install debugging symbols, which"
echo "will allow you to debug code in the system libraries. Most developers"
echo "won't need these symbols."
echo -n "Do you want me to install them for you (y/N) "
if yes_no 1; then
  echo "Installing debugging symbols."
else
  echo "Skipping installation of debugging symbols."
  dbg_list=
fi

sudo apt-get update

# We initially run "apt-get" with the --reinstall option and parse its output.
# This way, we can find all the packages that need to be newly installed
# without accidentally promoting any packages from "auto" to "manual".
# We then re-run "apt-get" with just the list of missing packages.
echo "Finding missing packages..."
new_list="$(yes n |
            LANG=C sudo apt-get install --reinstall \
                         ${dev_list} ${lib_list} ${dbg_list} \
                         $([ "$(uname -m)" = x86_64 ] && echo ${cmp_list}) \
                         |
            sed -e '1,/The following NEW packages will be installed:/d;s/^  //;t;d')"

echo "Installing missing packages..."
sudo apt-get install ${new_list}

# Some operating systems already ship gold
# (on Debian, you can probably do "apt-get install binutils-gold" to get it),
# but though Ubuntu toyed with shipping it, they haven't yet.
# So just install from source if it isn't the default linker.

case `ld --version` in
*gold*) ;;
* )
  # FIXME: avoid installing as /usr/bin/ld
  echo "Gold is a new linker that links Chrome 5x faster than ld."
  echo "Don't use it if you need to link other apps (e.g. valgrind, wine)"
  echo -n "REPLACE SYSTEM LINKER ld with gold and back up ld as ld.orig? (y/N) "
  if yes_no 1; then
    echo "Building binutils."
    install_gold || exit 99
  else
    echo "Not installing gold."
  fi
esac

# Install 32bit backwards compatibility support for 64bit systems
if [ "$(uname -m)" = x86_64 ]; then
  echo "Installing 32bit libraries that are not already provided by the system"
  echo
  echo "While we only need to install a relatively small number of library"
  echo "files, we temporarily need to download a lot of large *.deb packages"
  echo "that contain these files. We will create new *.deb packages that"
  echo "include just the 32bit libraries. These files will then be found on"
  echo "your system in places like /lib32, /usr/lib32, /usr/lib/debug/lib32,"
  echo "/usr/lib/debug/usr/lib32. If you ever need to uninstall these files,"
  echo "look for packages named *-ia32.deb."
  echo "Do you want me to download all packages needed to build new 32bit"
  echo -n "package files (Y/n) "
  if ! yes_no 0; then
    echo "Exiting without installing any 32bit libraries."
    exit 0
  fi
  tmp=/tmp/install-32bit.$$
  trap 'rm -rf "${tmp}"' EXIT INT TERM QUIT
  mkdir -p "${tmp}/apt/lists/partial" "${tmp}/cache" "${tmp}/partial"
  touch "${tmp}/status"

  [ -r /etc/apt/apt.conf ] && cp /etc/apt/apt.conf "${tmp}/apt/"
  cat >>"${tmp}/apt/apt.conf" <<-EOF
	Apt::Architecture "i386";
	Dir::Cache "${tmp}/cache";
	Dir::Cache::Archives "${tmp}/";
	Dir::State::Lists "${tmp}/apt/lists/";
	Dir::State::status "${tmp}/status";
	EOF

  # Download 32bit packages
  echo "Computing list of available 32bit packages..."
  apt-get -c="${tmp}/apt/apt.conf" update

  echo "Downloading available 32bit packages..."
  apt-get -c="${tmp}/apt/apt.conf" \
          --yes --download-only --force-yes --reinstall  install \
          ${lib_list} ${dbg_list}

  # Open packages, remove everything that is not a library, move the
  # library to a lib32 directory and package everything as a *.deb file.
  echo "Repackaging and installing 32bit packages for use on 64bit systems..."
  for i in ${lib_list} ${dbg_list}; do
    orig="$(echo "${tmp}/${i}"_*_i386.deb)"
    compat="$(echo "${orig}" |
              sed -e 's,\(_[^_/]*_\)i386\(.deb\),-ia32\1amd64\2,')"
    rm -rf "${tmp}/staging"
    msg="$(fakeroot -u sh -exc '
      # Unpack 32bit Debian archive
      umask 022
      mkdir -p "'"${tmp}"'/staging/dpkg/DEBIAN"
      cd "'"${tmp}"'/staging"
      ar x "'${orig}'"
      tar zCfx dpkg data.tar.gz
      tar zCfx dpkg/DEBIAN control.tar.gz

      # Rename package, change architecture, remove dependencies
      sed -i -e "s/\(Package:.*\)/\1-ia32/"       \
             -e "s/\(Architecture:\).*/\1 amd64/" \
             -e "s/\(Depends:\).*/\1 ia32-libs/"  \
             -e "/Recommends/d"                   \
             -e "/Conflicts/d"                    \
          dpkg/DEBIAN/control

      # Only keep files that live in "lib" directories
      sed -i -e "/\/lib64\//d" -e "/\/.?bin\//d" \
             -e "s,\([ /]lib\)/,\132/g,;t1;d;:1" \
             -e "s,^/usr/lib32/debug\(.*/lib32\),/usr/lib/debug\1," \
          dpkg/DEBIAN/md5sums

      # Re-run ldconfig after installation/removal
      { echo "#!/bin/sh"; echo "[ \"x\$1\" = xconfigure ]&&ldconfig||:"; } \
        >dpkg/DEBIAN/postinst
      { echo "#!/bin/sh"; echo "[ \"x\$1\" = xremove ]&&ldconfig||:"; } \
        >dpkg/DEBIAN/postrm
      chmod 755 dpkg/DEBIAN/postinst dpkg/DEBIAN/postrm

      # Remove any other control files
      find dpkg/DEBIAN -mindepth 1 "(" -name control -o -name md5sums -o \
                       -name postinst -o -name postrm ")" -o -print |
        xargs -r rm -rf

      # Remove any files/dirs that live outside of "lib" directories
      find dpkg -mindepth 1 "(" -name DEBIAN -o -name lib ")" -prune -o \
                -print | tac | xargs -r -n 1 sh -c \
                "rm \$0 2>/dev/null || rmdir \$0 2>/dev/null || : "
      find dpkg -name lib64 -o -name bin -o -name "?bin" |
        tac | xargs -r rm -rf

      # Rename lib to lib32, but keep debug symbols in /usr/lib/debug/usr/lib32
      # That is where gdb looks for them.
      find dpkg -type d -o -path "*/lib/*" -print |
        xargs -r -n 1 sh -c "
          i=\$(echo \"\${0}\" |
               sed -e s,/lib/,/lib32/,g \
               -e s,/usr/lib32/debug\\\\\(.*/lib32\\\\\),/usr/lib/debug\\\\1,);
          mkdir -p \"\${i%/*}\";
          mv \"\${0}\" \"\${i}\""

      # Prune any empty directories
      find dpkg -type d | tac | xargs -r -n 1 rmdir 2>/dev/null || :

      # Create our own Debian package
      cd ..
      dpkg --build staging/dpkg .' 2>&1)"
    compat="$(eval echo $(echo "${compat}" |
                          sed -e 's,_[^_/]*_amd64.deb,_*_amd64.deb,'))"
    [ -r "${compat}" ] || {
      echo "${msg}" >&2
      echo "Failed to build new Debian archive!" >&2
      exit 1
    }

    msg="$(sudo dpkg -i "${compat}" 2>&1)" && {
        echo "Installed ${compat##*/}"
      } || {
        # echo "${msg}" >&2
        echo "Skipped ${compat##*/}"
      }
  done

  # Add symbolic links for developing 32bit code
  echo "Adding missing symbolic links, enabling 32bit code development..."
  for i in $(find /lib32 /usr/lib32 -maxdepth 1 -name \*.so.\* |
             sed -e 's/[.]so[.][0-9].*/.so/' |
             sort -u); do
    [ "x${i##*/}" = "xld-linux.so" ] && continue
    [ -r "$i" ] && continue
    j="$(ls "$i."* | sed -e 's/.*[.]so[.]\([^.]*\)$/\1/;t;d' |
         sort -n | tail -n 1)"
    [ -r "$i.$j" ] || continue
    sudo ln -s "${i##*/}.$j" "$i"
  done
fi
