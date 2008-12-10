#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# chromium_extension.py

import logging
import optparse
import os
import re
import shutil
import sys
import zipfile

ignore_dirs = [".svn", "CVS"]
ignore_files = [re.compile(".*~")]

class ExtensionDir:
  def __init__(self, path):
    self._root = os.path.abspath(path)
    self._dirs = []
    self._files = []
    for root, dirs, files in os.walk(path, topdown=True):
      for dir in ignore_dirs:
        if dir in dirs:
          dirs.remove(dir)
      root = os.path.abspath(root)
      for dir in dirs:
        self._dirs.append(os.path.join(root, dir))
      for f in files:
        for match in ignore_files:
          if not match.match(f):
            self._files.append(os.path.join(root, f))

  def validate(self):
    if os.path.join(self._root, "manifest") not in self._files:
      logging.error("package is missing a valid manifest")
      return False
    return True

  def writeToPackage(self, path):
    if not self.validate():
      return False
    try:
      if os.path.exists(path):
        os.remove(path)
      shutil.copy(os.path.join(self._root, "manifest"), path)
      # This is a bit odd - we're actually appending a new zip file to the end
      # of the manifest.  Believe it or not, this is actually an explicit 
      # feature of the zipfile package, and most zip utilities (this library
      # and three others I tried) can still read the underlying zip file.
      zip = zipfile.ZipFile(path, "a")
      (root, dir) = os.path.split(self._root)
      root_len = len(root)
      for file in self._files:
        arcname = file[root_len+1:]
        logging.debug("%s: %s" % (arcname, file))
        zip.write(file, arcname)
      zip.close()
      logging.info("created extension package %s" % path)
    except IOError, (errno, strerror):
      logging.error("error creating extension %s (%d, %s)" % (path, errno,
                    strerror))
      try:
        if os.path.exists(path):
          os.remove(path)
      except:
        pass
      return False
    return True


class ExtensionPackage:
  def __init__(self, path):
    zip = zipfile.ZipFile(path)
    error = zip.testzip()
    if error:
      logging.error("error reading extension: %s", error)
      return
    logging.info("%s contents:" % path)
    files = zip.namelist()
    for f in files:
      logging.info(f)


def Run():
  logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
  
  parser = optparse.OptionParser("usage: %prog --indir=<dir> --outfile=<file>")
  parser.add_option("", "--indir",
                    help="an input directory where the extension lives")
  parser.add_option("", "--outfile",
                    help="extension package filename to create")
  (options, args) = parser.parse_args()
  if not options.indir:
    parser.error("missing required option --indir")
  if not options.outfile:
    parser.error("missing required option --outfile")
  ext = ExtensionDir(options.indir)
  ext.writeToPackage(options.outfile)
  pkg = ExtensionPackage(options.outfile)
  return 0


if __name__ == "__main__":
  retcode = Run()
  sys.exit(retcode)
