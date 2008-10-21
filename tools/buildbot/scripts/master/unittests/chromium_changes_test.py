#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import chromium_changes
import xml.dom.minidom

class GoogleChangesTest(unittest.TestCase):

  def test_create_changes_ignores_gvn_entry(self):
    svn_poller = chromium_changes.SVNPoller("svn://localhost/trunk")

    log_string = """
    <log>
      <logentry revision="18330">
        <author>author1</author>
        <date>2007-12-20T18:25:55.104681Z</date>
        <paths>
          <path action="D">/changes/author1/force_build</path>
          <path action="M">/trunk/chrome/dummy_file1.txt</path>
        </paths>
        <msg>message 1</msg>
      </logentry>
      <logentry revision="17871">
        <author>author2</author>
        <date>2007-12-16T02:15:54.054167Z</date>
        <paths>
          <path action="D">/changes/author2/t9m9</path>
          <path action="A">/trunk/chrome/dummy_file2.txt</path>
        </paths>
        <msg>message 2</msg>
      </logentry>
    </log>
    """
    # method has a necessary side-effect to proceed.
    svn_poller.determine_prefix(log_string)
    log_entries = xml.dom.minidom.parseString(log_string) \
                                              .getElementsByTagName('logentry')

    changes = svn_poller.create_changes(log_entries)
    self.assertEqual(2, len(changes))
    self.assertEqual(18330, changes[0].revision)
    self.assertEqual(['trunk/chrome/dummy_file1.txt'], changes[0].files)
    self.assertEqual(17871, changes[1].revision)
    self.assertEqual(['trunk/chrome/dummy_file2.txt'], changes[1].files)


if __name__ == '__main__':
  unittest.main()
