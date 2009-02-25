#!/usr/bin/python

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: action_jsconfig.py JS_ENGINE OUTPUT_DIR CONFIG_H_IN FILES_TO_COPY
# JS_ENGINE may be v8 at present.  jsc will be added in the future.
# CONFIG_H_DIR is the directory to put config.h in.
# OUTPUT_DIR is the directory to put other files in.
# CONFIG_H_IN is the path to config.h.in upon which config.h will be based.
# FILES_TO_COPY is a list of additional headers to be copied.  It may be empty.

import errno
import os
import os.path
import shutil
import sys

assert len(sys.argv) >= 5
js_engine = sys.argv[1]
config_h_dir = sys.argv[2]
output_dir = sys.argv[3]
config_h_in_path = sys.argv[4]
files_to_copy = sys.argv[5:]

config_h_path = os.path.join(config_h_dir, 'config.h')

assert js_engine == 'v8'

config_h_in_file = open(config_h_in_path)
config_h_in_contents = config_h_in_file.read()
config_h_in_file.close()

config_h_file = open(config_h_path, 'w')
print >>config_h_file, config_h_in_contents
if js_engine == 'v8':
  print >>config_h_file, '#define WTF_USE_V8_BINDING 1'
  print >>config_h_file, '#define WTF_USE_NPOBJECT 1'
config_h_file.close()

for file in files_to_copy:
  # This is not strictly right for jsc headers, which will want to be in one
  # more subdirectory named JavaScriptCore.
  basename = os.path.basename(file)
  destination = os.path.join(output_dir, basename)
  shutil.copy(file, destination)
