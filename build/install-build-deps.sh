#!/bin/bash -e

# Script to install everything needed to build chromium (well, ideally, anyway)
# See http://code.google.com/p/chromium/wiki/LinuxBuildInstructions
# and http://code.google.com/p/chromium/wiki/LinuxBuild64Bit

if ! egrep -q "Ubuntu 8.04|Ubuntu 8.10" /etc/issue; then
  echo "Only Ubuntu 8.04 and 8.10 are currently supported" >&2
  exit 1
fi

if ! uname -m | egrep -q "i686|x86_64"; then
  echo "Only x86 architectures are currently supported" >&2
  exit
fi

if [ "x$(id -u)" != x0 ]; then
  echo "Running as non-root user."
  echo "You might have to enter your password one or more times for 'sudo'."
fi

# Packages need for development
dev_list="subversion pkg-config python perl g++ g++-multilib bison flex gperf
          libnss3-dev libglib2.0-dev libgtk2.0-dev libnspr4-dev libsqlite3-dev
          wdiff lighttpd php5-cgi msttcorefonts sun-java6-fonts"

# Full list of required run-time libraries
lib_list="libatk1.0-0 libc6 libcairo2 libexpat1 libfontconfig1 libfreetype6
          libglib2.0-0 libgtk2.0-0 libnspr4-0d libnss3-1d libpango1.0-0
          libpcre3 libpixman-1-0 libpng12-0 libstdc++6 libsqlite3-0 libx11-6
          libxau6 libxcb-xlib0 libxcb1 libxcomposite1 libxcursor1 libxdamage1
          libxdmcp6 libxext6 libxfixes3 libxi6 libxinerama1 libxrandr2
          libxrender1 zlib1g"

# Debugging symbols for all of the run-time libraries
dbg_list="libatk1.0-dbg libc6-dbg libcairo2-dbg libfontconfig1-dbg
          libglib2.0-0-dbg libgtk2.0-0-dbg libnspr4-0d-dbg libnss3-1d-dbg
          libpango1.0-0-dbg libpcre3-dbg libpixman-1-0-dbg libx11-6-dbg
          libxau6-dbg libxcb-xlib0-dbg libxcb1-dbg libxcomposite1-dbg
          libxcursor1-dbg libxdamage1-dbg libxdmcp6-dbg libxext6-dbg
          libxfixes3-dbg libxi6-dbg libxinerama1-dbg libxrandr2-dbg
          libxrender1-dbg zlib1g-dbg"

# Standard 32bit compatibility libraries
cmp_list="ia32-libs lib32stdc++6 lib32z1 lib32z1-dev libc6-dev-i386 libc6-i386"

echo "Updating list of available packages..."
sudo apt-get update

# We initially run "apt-get" with the --reinstall option and parse its output.
# This way, we can find all the packages that need to be newly installed
# without accidentally promoting any packages from "auto" to "manual".
# We then re-run "apt-get" with just the list of missing packages.
echo "Finding missing packages..."
new_list="$(yes n |
            sudo apt-get install --reinstall \
                         ${pkg_list} ${lib_list} ${dbg_list} \
                         $([ "$(uname -m)" = x86_64 ] && echo ${cmp_list}) \
                         2>/dev/null |
            sed -e 's/^  //;t;d')"

echo "Installing missing packages..."
sudo apt-get install ${new_list}

# Install 32bit backwards compatibility support for 64bit systems
if [ "$(uname -m)" = x86_64 ]; then
  echo "Installing 32bit libraries that are not already provided by the system"
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
