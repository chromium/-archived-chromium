#!/bin/sh

export THISDIR=`dirname $0`
PYTHONPATH=$THISDIR/../../webkit/tools/layout_tests/:$THISDIR/../purify:$THISDIR/../python "./chrome_tests.py" "$@"
