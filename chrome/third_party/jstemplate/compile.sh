#!/bin/sh
# A shell script that combines the javascript files needed by jstemplate into
# a single file.

SRCS="base.js dom.js util.js jstemplate.js"
OUT="jstemplate_compiled.js"

# TODO(tc): We should use an open source JS compressor/compiler to reduce
# the size of our JS code.
cat $SRCS > $OUT
