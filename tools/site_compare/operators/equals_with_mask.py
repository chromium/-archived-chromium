#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Compare two images for equality, subject to a mask."""

from PIL import Image
from PIL import ImageChops

import os.path


def Compare(file1, file2, **kwargs):
  """Compares two images to see if they're identical subject to a mask.
  
  An optional directory containing masks is supplied. If a mask exists
  which matches file1's name, areas under the mask where it's black
  are ignored.
  
  Args:
    file1: path to first image to compare
    file2: path to second image to compare
    kwargs: ["maskdir"] contains the directory holding the masks
    
  Returns:
    None if the images are identical
    A tuple of (errorstring, image) if they're not
  """
  
  maskdir = None
  if "maskdir" in kwargs:
    maskdir = kwargs["maskdir"]
  
  im1 = Image.open(file1)
  im2 = Image.open(file2)
  
  if im1.size != im2.size:
    return ("The images are of different size (%r vs %r)" %
            (im1.size, im2.size), im1)

  diff = ImageChops.difference(im1, im2)
  
  if maskdir:
    maskfile = os.path.join(maskdir, os.path.basename(file1))
    if os.path.exists(maskfile):
      mask = Image.open(maskfile)
      
      if mask.size != im1.size:
        return ("The mask is of a different size than the images (%r vs %r)" %
                (mask.size, im1.size), mask)
      
      diff = ImageChops.multiply(diff, mask.convert(diff.mode))
  
  if max(diff.getextrema()) != (0, 0):
    return ("The images differ", diff)
  else:
    return None
  
  
  