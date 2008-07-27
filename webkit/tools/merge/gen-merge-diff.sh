#!/bin/sh
#
# Copyright 2007 Google Inc.
# All rights reserved.
#
# This script expects to live in chrome/tools/branch/.  If it's ever moved,
# the value of |root| will need to be changed below.
#
# Generate a helper script that does a 3-way diff on each forked WebKit file
# you give it.  The diff will be among:
#  base: original third_party file from a Chrome trunk checkout
#  diff1: updated third_party file from the merge branch
#  diff2: forked file in webkit/pending or wherever.
#
# Run this script from the merge branch.  Give it the path to the root of your
# trunk checkout (the directory containing src and data directories) as the
# first argument, and a list of forked files (e.g. from either copy of
# webkit/pending/) as the second argument.
#
# Example usage:
#  ./gen-merge-diff.sh /cygdrive/c/src/chrome/trunk webkit/pending/*.*
#
# Once the helper script is generated, you can run it to do the 3-way diff.

wkfilelist=/tmp/webkit-branch-merge/third_party_list.txt

execdir=`dirname $0`
root=$execdir/../..

origtrunk=$1
origroot=$origtrunk
shift

if [[ $# -lt 1 || ! -d "$origroot/third_party" ]]; then
  echo "Usage: $0 <path-to-the-base-dir-of-chrome-trunk> [list-of-forked-files]"
  echo "Example: $0 /cygdrive/c/src/chrome/trunk webkit/pending/*.*"
  exit 1
fi

if which p4merge.exe >& /dev/null; then
  merge_exe=p4merge.exe
else
  merge_exe="/cygdrive/c/Program Files/Perforce/p4merge.exe"
  if [ ! -e "$merge_exe" ]; then
    echo "WARNING: It looks like you don't have p4merge installed."
    echo "You must edit merge-diff.sh and change the diff3way function to"
    echo "point at a valid merge tool."
  fi
fi

if [ -e $wkfilelist ]; then
  echo "Using cached file listing of third_party."
  echo "WARNING: If the cached file is not up to date, you should execute:"
  echo "   rm $wkfilelist"
  echo "and run this script again."
else
  echo "Creating file listing of third_party..."
  (cd $root && find third_party -type f | grep -v '/\.svn' | grep -v 'ForwardingHeaders' | grep -v 'DerivedSources' > $wkfilelist)
fi

# Prepare the helper script.

cat > merge-diff.sh <<EOF
#!/bin/sh

function diff3way {
  a=\`cygpath -wa \$1\`
  b=\`cygpath -wa \$2\`
  c=\`cygpath -wa \$3\`
  "$merge_exe" "\$a" "\$b" "\$c" "\$c"
}

EOF

chmod +x merge-diff.sh

# Now add all the diff commands to the helper script.

for each in "$@"; do
  filename=`echo $each | sed -e "s,.*/,,g"`
  tpfile=`grep "/\<$filename\>" $wkfilelist`
  if [ "$tpfile" != "" ]; then
    # Only run the 3-way diff if the upstream file has changed
    ignored=`diff "$origroot/$tpfile" "$root/$tpfile"`
    if [ $? != 0 ]; then
      echo "diff3way \"$origroot/$tpfile\" \"$root/$tpfile\" \"$each\"" >> merge-diff.sh
    fi
  fi
done

cat << EOF

======================
Created: merge-diff.sh

Please edit this file to make sure it is correct.  Then run it with
'./merge-diff.sh'.
EOF
