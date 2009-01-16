#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit test suite that collects all test cases for GRIT.'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import unittest


# TODO(joi) Use unittest.defaultTestLoader to automatically load tests
# from modules. Iterating over the directory and importing could then
# automate this all the way, if desired.


class TestSuiteAll(unittest.TestSuite):
  def __init__(self):
    super(type(self), self).__init__()
    # Imports placed here to prevent circular imports.
    from grit import grd_reader_unittest
    from grit import grit_runner_unittest
    from grit.node import base_unittest
    from grit.node import io_unittest
    from grit import clique_unittest
    from grit.node import misc_unittest
    from grit.gather import rc_unittest
    from grit.gather import tr_html_unittest
    from grit.node import message_unittest
    from grit import tclib_unittest
    import grit.format.rc_unittest
    import grit.format.data_pack_unittest
    from grit.tool import rc2grd_unittest
    from grit.tool import transl2tc_unittest
    from grit.gather import txt_unittest
    from grit.gather import admin_template_unittest
    from grit import xtb_reader_unittest
    from grit import util_unittest
    from grit.tool import preprocess_unittest
    from grit.tool import postprocess_unittest
    from grit import shortcuts_unittests
    from grit.gather import muppet_strings_unittest
    from grit.node.custom import filename_unittest
    
    test_classes = [
      base_unittest.NodeUnittest,
      io_unittest.FileNodeUnittest,
      grit_runner_unittest.OptionArgsUnittest,
      grd_reader_unittest.GrdReaderUnittest,
      clique_unittest.MessageCliqueUnittest,
      misc_unittest.GritNodeUnittest,
      rc_unittest.RcUnittest,
      tr_html_unittest.ParserUnittest,
      tr_html_unittest.TrHtmlUnittest,
      message_unittest.MessageUnittest,
      tclib_unittest.TclibUnittest,
      grit.format.rc_unittest.FormatRcUnittest,
      grit.format.data_pack_unittest.FormatDataPackUnittest,
      rc2grd_unittest.Rc2GrdUnittest,
      transl2tc_unittest.TranslationToTcUnittest,
      txt_unittest.TxtUnittest,
      admin_template_unittest.AdmGathererUnittest,
      xtb_reader_unittest.XtbReaderUnittest,
      misc_unittest.IfNodeUnittest,
      util_unittest.UtilUnittest,
      preprocess_unittest.PreProcessingUnittest,
      postprocess_unittest.PostProcessingUnittest,
      misc_unittest.ReleaseNodeUnittest,
      shortcuts_unittests.ShortcutsUnittest,
      muppet_strings_unittest.MuppetStringsUnittest,
      filename_unittest.WindowsFilenameUnittest,
      # add test classes here...
    ]

    for test_class in test_classes:
      self.addTest(unittest.makeSuite(test_class))


if __name__ == '__main__':
  unittest.TextTestRunner(verbosity=2).run(TestSuiteAll())

