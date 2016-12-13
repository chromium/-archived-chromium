#!/bin/sh
# Script to run all tests under tools/valgrind/chrome_tests.sh
# in a loop looking for rare/flaky valgrind warnings, and
# generate suppressions for them, to be later filed as bugs
# and added to our suppressions file.
#
# FIXME: Layout tests are a bit funny - they have their own 
# sharding control, and should probably be tweaked to obey
# GTEST_SHARD_INDEX/GTEST_TOTAL_SHARDS like the rest,
# but they take days and days to run, so they are left
# out of this script.

if test ! -d chrome
then
  echo "Please run from parent directory of chrome and build directories"
  exit 1
fi

set -x
set -e

# Regexp to match any valgrind error
PATTERN="ERROR SUMMARY: [^0]|are definitely|uninitialised|Unhandled exception:\
Invalid read|Invalid write|Invalid free|Source and desti|Mismatched free|\
unaddressable byte|vex x86"

# List of test executables to build and run
# You might want to trim this list down to just one if you have an idea
# which test has interesting problems (e.g. ui_tests)
TESTS="base_unittests googleurl_unittests ipc_tests media_unittests \
       net_unittests printing_unittests startup_tests \
       test_shell_tests ui_tests unit_tests"

# Build the tests (if we know how)
OS=`uname`
case $OS in
Linux)
  cd build
  hammer $TESTS
  cd ..
  ;;
*) echo "don't know how to build on os $OS"
  ;;
esac

# Divide each test suite up into 50 shards, as first step
# in tracking down exact source of errors.
export GTEST_TOTAL_SHARDS=50

iter=0
while test $iter -lt 1000
do
  for testname in $TESTS
  do
    export GTEST_SHARD_INDEX=0
    while test $GTEST_SHARD_INDEX -lt $GTEST_TOTAL_SHARDS
    do
      i=$GTEST_SHARD_INDEX
      sh tools/valgrind/chrome_tests.sh -t ${testname} --generate_suppressions\
                                         > ${testname}_$i.vlog 2>&1
      GTEST_SHARD_INDEX=`expr $GTEST_SHARD_INDEX + 1`
    done
  done

  # Save any interesting log files from this iteration
  # Also show interesting lines on stdout, to make tail -f more interesting
  if egrep "$PATTERN" *.vlog
  then
    mkdir -p shard-results/$iter
    mv `egrep -l "$PATTERN" *.vlog` shard-results/$iter
  fi

  rm *.vlog
  iter=`expr $iter + 1`
done
