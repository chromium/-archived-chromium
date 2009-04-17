#!/bin/sh

# A simple shell script for creating necessary zip files for ChromeBot runs
# from an output directory.
# Pass the path to the output directory you wish to package.

if [ $# = 0 ]; then
  echo "usage: make_chromebot_zip.sh path/to/release/dir [output-name]"
  exit 1
fi

tools_dir=$(dirname "$0")
release_dir="$1"

# Create chrome build zip file
files=$(cat "$tools_dir/FILES")
test_files=( reliability_tests.exe automated_ui_tests.exe )

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
for f in ${test_files[@]}; do
  cp -r --parents "$f" "$output_abs"
done
popd

zip -r $output.zip $output

# Create chrome symbol zip file
sym_files=( chrome_dll.pdb chrome_exe.pdb )

sym_output=${2:-chrome-win32-syms}
rm -fr $sym_output $sym_output.zip
mkdir $sym_output

# Again, use cp --parents to copy full relative directory.  Since we need the
# relative directory for the zip, change into the release dir.
sym_output_abs=`cygpath -a $sym_output`
pushd "$release_dir"
for f in ${sym_files[@]}; do
  cp -r --parents "$f" "$sym_output_abs"
done
popd

zip -r $sym_output.zip $sym_output
