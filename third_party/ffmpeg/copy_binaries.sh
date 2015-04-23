#!/bin/bash
#
# This is meant to replicate the behavior of cp, except that it will not fail
# if the source files are not present.  Something like
# "cp ${SOURCES} ${DEST} || true" would also have worked except that
# gyp does not allow for specifying || in the action, and the windows
# cygwin envrionment does not include "true" making a script like this the
# least ugly solution.

SOURCES=""
DESTINATION=""

# Shift off every argument but the last and consider them the sources.
# It would have probably been easier to put the destination first, but
# this is not too hard and it replicates the argument ordering of cp.
while (( "$#" != 1 )); do
  SOURCES="$SOURCES $1"
  shift
done

DESTINATION=$1

# Early out if there was not enough parameters to discern a destination.
# Also fail the command because this means we are being invoked incorrectly.
if test -z "$DESTINATION"; then
  echo "ERROR: Destination empty."
  exit 1
fi

# Only try to copy the source file if it exists.  It is not an error
# if the input does not exist; we just silently ignore the input.
for i in $SOURCES; do
  if test -f $i; then
    cp -v -f $i $DESTINATION
  fi
done

# Make sure we always succeed.
exit 0
