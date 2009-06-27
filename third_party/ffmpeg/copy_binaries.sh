#!/bin/bash
#
# Copies FFmpeg binaries due to gyp limitations on how much you can cram
# into a rules action.

SOURCE=$1
DESTINATION=$2

# Our cygwin environment is very limited: "if" isn't available.
test -f $SOURCE && cp -v -f $SOURCE $DESTINATION

# Make sure we always succeed.
exit 0
