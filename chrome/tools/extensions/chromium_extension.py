#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# chromium_extension.py

import array
import hashlib
import logging
import optparse
import os
import random
import re
import shutil
import sys
import zipfile

if sys.version_info < (2, 6):
  import simplejson as json
else:
  import json


ignore_dirs = [".svn", "CVS"]
ignore_files = [re.compile(".*~")]

MANIFEST_FILENAME = "manifest.json"

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
    if os.path.join(self._root, MANIFEST_FILENAME) not in self._files:
      logging.error("package is missing a valid %s file" % MANIFEST_FILENAME)
      return False
    return True

  def writeToPackage(self, path):
    if not self.validate():
      return False
    try:
      f = open(os.path.join(self._root, MANIFEST_FILENAME), "r")
      manifest = json.load(f)
      f.close()

      # Temporary hack: If the manifest doesn't have an ID, generate a random
      # one. This is to make it easier for people to play with the extension
      # system while we don't have the real ID mechanism in place.
      if not "id" in manifest:
        random_id = ""
        for i in range(0, 40):
          random_id += "0123456789ABCDEF"[random.randrange(0, 15)]
        logging.info("Generated extension ID: %s" % random_id)
        manifest["id"] = random_id;
        f = open(os.path.join(self._root, MANIFEST_FILENAME), "w")
        f.write(json.dumps(manifest, sort_keys=True, indent=2));
        f.close();

      zip_path = path + ".zip"
      if os.path.exists(zip_path):
        os.remove(zip_path)
      zip = zipfile.ZipFile(zip_path, "w")
      (root, dir) = os.path.split(self._root)
      root_len = len(self._root)
      for file in self._files:
        arcname = file[root_len+1:]
        logging.debug("%s: %s" % (arcname, file))
        zip.write(file, arcname)
      zip.close()

      zip = open(zip_path, mode="rb")
      hash = hashlib.sha256()
      while True:
        buf = zip.read(32 * 1024)
        if not len(buf):
          break
        hash.update(buf)
      zip.close()

      manifest["zip_hash"] = hash.hexdigest()

      # This is a bit odd - we're actually appending a new zip file to the end
      # of the manifest.  Believe it or not, this is actually an explicit
      # feature of the zip format, and many zip utilities (this library
      # and three others I tried) can still read the underlying zip file.
      if os.path.exists(path):
        os.remove(path)
      out = open(path, "wb")
      out.write("Cr24")  # Extension file magic number
      # The rest of the header is currently made up of three ints:
      # version, header size, manifest size
      header = array.array("i")
      header.append(1)  # version
      header.append(16)  # header size
      manifest_json = json.dumps(manifest);
      header.append(len(manifest_json))  # manifest size
      header.tofile(out)
      out.write(manifest_json);
      zip = open(zip_path, "rb")
      while True:
        buf = zip.read(32 * 1024)
        if not len(buf):
          break
        out.write(buf)
      zip.close()
      out.close()

      os.remove(zip_path)

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
