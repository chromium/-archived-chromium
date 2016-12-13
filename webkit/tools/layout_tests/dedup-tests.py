#!/usr/bin/python

"""dedup-tests -- print test results duplicated between win and linux.

Because the outputs are very similar, we fall back on Windows outputs
if there isn't an expected output for Linux layout tests.  This means
that any file that is duplicated between the Linux and Windows directories
is redundant.

This command dumps out all such files.  You can use it like:
  dedup-tests.py   # print out the bad files
  dedup-tests.py | xargs git rm   # delete the bad files
"""

import collections
import os.path
import subprocess
import sys

# A map of file hash => set of all files with that hash.
hashes = collections.defaultdict(set)

# Fill in the map.
cmd = ['git', 'ls-tree', '-r', 'HEAD', 'webkit/data/layout_tests/']
try:
  git = subprocess.Popen(cmd, stdout=subprocess.PIPE)
except OSError, e:
  if e.errno == 2:  # No such file or directory.
    print >>sys.stderr, "Error: 'No such file' when running git."
    print >>sys.stderr, "This script requires git."
    sys.exit(1)
  raise e

for line in git.stdout:
  attrs, file = line.strip().split('\t')
  _, _, hash = attrs.split(' ')
  hashes[hash].add(file)

# Dump out duplicated files.
for cluster in hashes.values():
  if len(cluster) < 2:
    continue
  for file in cluster:
    if '/chromium-linux/' in file:
      if file.replace('/chromium-linux/', '/chromium-win/') in cluster:
        print file
