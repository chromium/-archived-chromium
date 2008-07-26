#!/bin/sh

# A simple shell script for creating a chrome zip from an output directory.
# Pass the path to the output directory you wish to package.

if [ $# = 0 ]; then
  echo "usage: make_zip.sh path/to/release/dir [output-name]"
  exit 1
fi

tools_dir=$(dirname "$0")
release_dir="$1"

files=$(cat "$tools_dir/FILES")

output=${2:-chrome-win32}
rm -fr $output $output.zip
mkdir $output

# Get the absolute path of the output directory.  We need it when copying
# files.
output_abs=`cygpath -a $output`

# Use cp --parents to copy full relative directory.  Since we need the
# relative directory for the zip, change into the release dir.
pushd "$release_dir"
# The file names in FILES may contain whitespace, e.g. 'First Run'.
# Change IFS setting so we only split words with '\n'
IFS_Default=$IFS
IFS=$'\n'
for f in ${files[@]}; do
  cp -r --parents "$f" "$output_abs"
done
IFS=$IFS_Default
popd

zip -r $output.zip $output
