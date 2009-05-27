#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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


"""Test for ezt_formatter."""


import os
import os.path
import ezt_formatter
import unittest

class EZTFormatterUnitTest(unittest.TestCase):
  def setUp(self):
    self.formatter = ezt_formatter.EZTFormatter('.')
    self.java = ezt_formatter.DoxygenJavascriptifier()

  def tearDown(self):
    os.rmdir(self.formatter.originals_directory)

  def CreateContentList(self, inner_content):
    content = ['[BOILERPLATE - DO NOT EDIT THIS BLOCK]']
    content += ['[END OF BOILERPLATE]']
    content += inner_content
    content += ['[BOILERPLATE - DO NOT EDIT THIS BLOCK]']
    content += ['[END OF BOILERPLATE]']
    return content

  def testJavascriptify(self):
    # test numbers
    content = ['int myInt()']
    content += ['myFloat(float f);']
    java_content = self.java.Javascriptify(content)
    self.assertTrue(java_content[0] == 'Number myInt()')
    self.assertTrue(java_content[1] == 'myFloat(Number f);')
    # test non-javascript tags
    content = ['pure test::myVar [pure virtual] Static']
    content += ['const void testFunction() [virtual] [pure]']
    content += ['testing [explicit]']
    content += ['virtual [protected] test;']
    content += ['static unsigned int function();']
    content += ['const float testing::test()=0']
    content += ['protected testing() [pure]']
    content += ['[pure]']
    java_content = self.java.Javascriptify(content)
    self.assertTrue(java_content[0] == ' test.myVar  Global')
    self.assertTrue(java_content[1] == '  testFunction()  ')
    self.assertTrue(java_content[2] == 'testing ')
    self.assertTrue(java_content[3] == '  test;')
    self.assertTrue(java_content[4] == '  Number function();')
    self.assertTrue(java_content[5] == ' Number testing.test()')
    self.assertTrue(java_content[6] == ' testing() ')
    self.assertFalse(java_content[7])
    # test static tag
    content = ['myTestFunction() [static]']
    java_content = self.java.Javascriptify(content)
    self.assertTrue(java_content[0] == ('myTestFunction() [<a class="doxygen-'
                                        'global">global function</a>]'))
    # test null
    content = ['return NULL;']
    java_content = self.java.Javascriptify(content)
    self.assertTrue(java_content[0] == 'return null;')
    # test Array
    content = ['std.vector&lt;test::arrayType&gt; myArray']
    java_content = self.java.Javascriptify(content)
    self.assertTrue(java_content[0] == 'Array myArray')

  def testJavascriptifyGlobal(self):
    content = '<li>static test'
    java_content = self.java.LabelStaticAsGlobal(content)
    self.assertTrue(java_content == ('<li> test [<a class="doxygen-global">'
                                     'global function</a>]\n'))
    content = '<li>abcdefg static myFunction(abc)<br>'
    java_content = self.java.LabelStaticAsGlobal(content)
    self.assertTrue(java_content == ('<li>abcdefg  myFunction(abc)<br> [<a '
                                     'class="doxygen-global">global function'
                                     '</a>]\n'))
    content = 'static abcde'
    java_content = self.java.LabelStaticAsGlobal(content)
    self.assertTrue(java_content == content)

  def testBackupCopy(self):
    test_file = 'test_backup_file'
    open(test_file, 'w')
    self.formatter.MakeBackupCopy(test_file)
    test_file_backup = os.path.join(self.formatter.originals_directory,
                                    test_file)
    self.assertTrue(os.path.exists(test_file))
    self.assertTrue(os.path.exists(test_file_backup))
    os.remove(test_file)
    os.remove(test_file_backup)

  def testPerformFunctionsOnFile(self):
    def ReplaceTest(lines_list):
      new_lines_list = []
      for line in lines_list:
        new_lines_list += [line.replace('test', 'pass')]
      return new_lines_list

    test_file = 'test_file'
    lines = ['test\n'] * 20
    outfile = open(test_file, 'w')
    outfile.writelines(lines)
    outfile.close()

    self.formatter.RegisterModifier(ReplaceTest)
    self.formatter.PerformFunctionsOnFile(test_file)

    infile = open(test_file, 'r')
    infile_lines = infile.readlines()
    infile.close()
    for in_line in infile_lines:
      self.assertTrue(in_line == 'pass\n')
    os.remove(test_file)

  def testRemoveNavigation(self):
    content = ['<div class="test">']
    content += ['<div>']
    content += ['<div>']
    content += ['<div class="tabs" name="additional information to parse">']
    content += ['<div>']
    content += ['miscellaneous tab information']
    content += ['</div>']
    content += ['</div>']
    content += ['</div>']
    content += ['</div>']
    content += ['</div>']
    fixed_content = self.formatter.RemoveNavigation(content)
    self.assertTrue(len(fixed_content) == 6)
    self.assertTrue(fixed_content[0] == '<div class="test">')
    self.assertTrue(fixed_content[1] == '<div>')
    self.assertTrue(fixed_content[2] == '<div>')
    self.assertTrue(fixed_content[3] == '</div>')
    self.assertTrue(fixed_content[4] == '</div>')
    self.assertTrue(fixed_content[5] == '</div>')

  def testFixBrackets(self):
    content = ['test1']
    content += ['test2[]']
    content += ['test[3]']
    content += ['test[[4]]']
    content = self.CreateContentList(content)
    fixed_content = self.formatter.FixBrackets(content)
    self.assertTrue(fixed_content[2] == 'test1')
    self.assertTrue(fixed_content[3] == 'test2[[]]')
    self.assertTrue(fixed_content[4] == 'test[[]3]')
    self.assertTrue(fixed_content[5] == 'test[[][[]4]]')

  def testRenameFiles(self):
    html_file = 'test.html'
    ezt_file = 'test.ezt'
    open(html_file, 'w')
    self.formatter.RenameFiles(html_file)
    self.assertFalse(os.path.exists(html_file))
    self.assertTrue(os.path.exists(ezt_file))
    originals_html_file = os.path.join(self.formatter.originals_directory,
                                       html_file)
    self.assertTrue(originals_html_file)
    # test functionality when file already exists
    open(html_file, 'w')
    self.formatter.RenameFiles(html_file)
    self.assertFalse(os.path.exists(html_file))
    self.assertTrue(os.path.exists(ezt_file))
    self.assertTrue(originals_html_file)
    os.remove(originals_html_file)
    os.remove(ezt_file)


if __name__ == '__main__':
  unittest.main()
