#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

try:
  import hashlib
  _new_md5 = hashlib.md5
except ImportError:
  import md5
  _new_md5 = md5.new

"""64-bit fingerprint support for strings.

Usage:
    from extern import FP
    print 'Fingerprint is %ld' % FP.FingerPrint('Hello world!')
"""


def UnsignedFingerPrint(str, encoding='utf-8'):
  """Generate a 64-bit fingerprint by taking the first half of the md5
  of the string."""
  hex128 = _new_md5(str).hexdigest()
  int64 = long(hex128[:16], 16)
  return int64

def FingerPrint(str, encoding='utf-8'):
  fp = UnsignedFingerPrint(str, encoding=encoding)
  # interpret fingerprint as signed longs
  if fp & 0x8000000000000000L:
    fp = - ((~fp & 0xFFFFFFFFFFFFFFFFL) + 1)
  return fp

