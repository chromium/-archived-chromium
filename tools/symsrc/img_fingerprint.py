#!/usr/bin/env python

# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This will retrieve an image's "fingerprint".  This is used when retrieving
the image from the symbol server.  The .dll (or cab compressed .dl_) or .exe
is expected at a path like:
 foo.dll/FINGERPRINT/foo.dll"""

import sys
import pefile

def GetImgFingerprint(filename):
  """Returns the fingerprint for an image file"""

  pe = pefile.PE(filename)
  return "%08X%06x" % (
    pe.FILE_HEADER.TimeDateStamp, pe.OPTIONAL_HEADER.SizeOfImage)


if __name__ == '__main__':
  if len(sys.argv) != 2:
    print "usage: file.dll"
    sys.exit(1)

  print GetImgFingerprint(sys.argv[1])
