#!/bin/sh
# A shell script that combines the javascript files needed by jstemplate into
# a single file.

SRCS="util.js jsevalcontext.js jstemplate.js exports.js"
OUT="jstemplate_compiled.js"

# TODO(tc): We should use an open source JS compressor/compiler to reduce
# the size of our JS code.

# Stripping // comments is trivial, so we'll do that at least.
echo "(function(){" > $OUT
cat $SRCS | grep -v '^ *//' >> $OUT
echo "})()" >> $OUT
