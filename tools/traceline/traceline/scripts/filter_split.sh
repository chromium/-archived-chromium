#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Runs filter_short on the individual splits of a json file, and puts
# everything back together into a single output json.  This is useful when you
# want to filter a large json file that would otherwise OOM Python.

echo "parseEvents([" > totalsplit
for f in split.*; do
  python scripts/filter_short.py "$f" |  tail -n +2 | head -n -1 >> totalsplit
done
echo "]);" >> totalsplit
