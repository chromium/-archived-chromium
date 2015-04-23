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


"""HTML formatter for ezt docs."""


import os
import os.path
import re
import shutil
import sys


class DoxygenJavascriptifier(object):
  """Convert C++ documentation into Javascript style.

  Documentation may be automatically generated using doxygen. However,
  doxygen only outputs C++ formatted documentation, that is it uses C++
  conventions for specifying many items even if the java option is selected in
  the configuration file.

  In order to make the documentation look like Javascript documentation,
  additional corrections must be made which this class takes care of.

  Since doxygen allows you to name functions and class whatever you wish, this
  class assumes that you have already chosen to name them with standard
  Javascript conventions (such as camel-case) and therefore does not try to
  correct that.

  Though many of the replacements are simply eliminated, they are left as
  individual items in the dictionary for clarity.

  Attributes:
    cpp_to_js: a list of c++ to javascript replacement tuples
    static_function_regex: regular expression describing a static function
        declaration
  """

  global_function_tag = '[<a class="doxygen-global">global function</a>]'
  static_function2_regex = re.compile(r'<li>(.*)static(.*)$')
  cpp_to_js = [(re.compile(r'::'), '.'),
               (re.compile(r'\[pure\]'), ''),
               (re.compile(r'\[pure virtual\]'), ''),
               (re.compile(r'\[virtual\]'), ''),
               (re.compile(r'\[protected\]'), ''),
               (re.compile(r'\[static\]'), global_function_tag),
               (re.compile(r'\[explicit\]'), ''),
               (re.compile(r'\bvirtual\b'), ''),
               (re.compile(r'\bpure\b'), ''),
               (re.compile(r'\bprotected\b'), ''),
               (re.compile(r'\bvoid\b'), ''),
               (re.compile(r'\bunsigned\b'), ''),
               (re.compile(r'\bstatic\b'), ''),
               (re.compile(r'\bStatic\b'), 'Global'),
               (re.compile(r'\bint\b'), 'Number'),
               (re.compile(r'\bfloat\b'), 'Number'),
               (re.compile(r'\bNULL\b'), 'null'),
               (re.compile(r'=0'), ''),
               # Replace "std.vector<class_name>" with "Array" since vectors
               # don't exist in Javascript. There is no type for Array in
               # Javascript.
               (re.compile(r'std.vector&lt;[\w\s<>"/.=+]*&gt;'), 'Array'),
               # Remove mention of Vectormath.Aos namespace since it is
               # invisible to Javascript.
               (re.compile(r'Vectormath.Aos.Vector3'), 'Vector3'),
               (re.compile(r'Vectormath.Aos.Vector4'), 'Vector4'),
               (re.compile(r'Vectormath.Aos.Point3'), 'Point3'),
               (re.compile(r'Vectormath.Aos.Matrix4'), 'Matrix4'),
               (re.compile(r'Vectormath.Aos.Quat'), 'Quat'),
               (re.compile(r'\bconst\b'), '')]

  def Javascriptify(self, lines_list):
    """Replaces C++-style items with more Javascript-like ones.

    This includes removing many identifiers such as pure, virtual, protected,
    static and explicit which are not used in Javascript and replacing
    notation such as '::' with its corresponding Javascript equivalent,
    '.' in this case.

    Args:
      lines_list: the list of lines to correct.
    Returns:
      the corrected lines in javascript format.
    """
    for index, line in enumerate(lines_list):
      line = self.LabelStaticAsGlobal(line)
      for cpp_expression, js_expression in self.cpp_to_js:
        line = re.sub(cpp_expression, js_expression, line)
      lines_list[index] = line
    return lines_list

  def LabelStaticAsGlobal(self, line):
    """If a function is static, we indicate it is a global function.

    Since static does not make sense for Javascript we refer to these
    functions as global. To designate global function, we place a
    [global function] tag after the function declaration. Doxygen by
    default leaves the static descriptor in front of the function so
    this function looks for the descriptor, removes it and puts the
    tag at the end.

    Args:
      line: the line to correct.
    Returns:
      the corrected line.
    """
    match = re.search(self.static_function2_regex, line)
    if match is not None:
      line = '<li>%s%s %s\n' % (match.group(1), match.group(2),
                                self.global_function_tag)
    return line


class EZTFormatter(object):
  """This class converts doxygen output into ezt ready files.

  When doxygen outputs files, it contains its own formatting such as adding
  navigation tabs to the top of each page. This class contains several
  functions for stripping down doxygen output and formatting doxygen output
  to be easily parsed by ezt.

  This class assumes that any header/footer boilerplate code (that is code
  added by the user rather than doxygen at the start or end of a file) is
  indicated by special indicator strings surrounding it. Namely at the
  start of boilerplate code: 'BOILERPLATE - DO NOT EDIT THIS BLOCK' and
  at the end of boilerplate code: 'END OF BOILERPLATE'.

  Attributes:
    content_start_regex: regular expression for end of boilerplate code
    content_end_regex: regular expression for start of boilerplate code
    div_regex: regular expression of html div
    div_end_regex: regular expression of closing html div
    navigation_regex: regular expression indicating doxygen navigation tabs
    html_suffix_regex: regular expression html file suffix
    current_directory: location of files being worked on
    originals_directory: directory to store copies of original html files
    modifiers: list of functions to apply to files passed in
  """
  content_start_regex = re.compile('END OF BOILERPLATE')
  content_end_regex = re.compile('BOILERPLATE - DO NOT EDIT THIS BLOCK')
  div_regex = re.compile('<div(.*)>')
  div_end_regex = re.compile('</div>')
  navigation_regex = re.compile('<div class=\"tabs\"(.*)>')
  html_suffix_regex = re.compile('\.html$')

  def __init__(self, current_directory):

    self.current_directory = current_directory
    self.originals_directory = os.path.join(current_directory, 'original_html')
    if not os.path.exists(self.originals_directory):
      os.mkdir(self.originals_directory)
    self.modifiers = []

  def RegisterModifier(self, modifier):
    """Adds the modifier function to the list of modifier functions.

    Args:
      modifier: the function modifies a list of strings of code
    """
    self.modifiers += [modifier]

  def MakeBackupCopy(self, input_file):
    """Makes backup copy of input_file by copying to originals directory.

    Args:
      input_file: basename of file to backup.
    """
    shutil.copy(os.path.join(self.current_directory, input_file),
                self.originals_directory)

  def PerformFunctionsOnFile(self, input_file):
    """Opens a file as a list, modifies the list and writes to same file.

    Args:
      input_file: the file being operated on.
    """
    infile = open(input_file, 'r')
    lines_list = infile.readlines()
    infile.close()

    for function in self.modifiers:
      lines_list = function(lines_list)

    outfile = open(input_file, 'w')
    outfile.writelines(lines_list)
    outfile.close()

  def FixBrackets(self, lines_list):
    """Adjust brackets to be ezt style.

    Fix the brackets such that any open bracket '[' becomes '[[]' in order to
    not conflict with the brackets ezt uses as special characters.

    EZT uses boilerplate code at the beginning and end of each file. When
    editing a file, however, we don't want to touch the boilerplate code.
    We can look for special indicators through regular expressions of
    where the content lies between the boilerplate sections and then only
    apply the desired function line by line to the html between those
    sections.

    This is a destructive operation and will write over the input file.

    Args:
      lines_list: the file to fix broken into lines.
    Returns:
      the lines_list with correctly formatted brackets.
    """
    outside_boilerplate = False
    for index, line in enumerate(lines_list):
      if self.content_end_regex.search(line) and outside_boilerplate:
        # We've reached the end of content; return our work
        return lines_list
      elif self.content_start_regex.search(line):
        # We've found the start of real content, end of boilerplate
        outside_boilerplate = True
      elif outside_boilerplate:
        # Inside real content; fix brackets
        lines_list[index] = line.replace('[', '[[]')
      else:
        pass  # Inside boilerplate.
    return lines_list

  def RemoveNavigation(self, lines_list):
    """Remove html defining navigation tabs.

    This function removes the lines between the <div class ="tabs">...</div>
    which will eliminate the navigation tabs Doxygen outputs at the top of
    its documentation. These are extraneous for Google's codesite which
    produces its own navigation using the ezt templates.

    Nested <div>'s are handled, but it currently expects no more than one
    <div>...</div> set per line.

    This is a destructive operation and will write over the input file.

    Args:
      lines_list: the lines of the file to fix.
    Returns:
      the lines_list reformatted to not include tabs div set.
    """
    start_index = -1
    div_queue = []
    start_end_pairs = {}
    for index, line in enumerate(lines_list):
      # Start by looking for the start of the navigation section
      if self.navigation_regex.search(line):
        start_index = index
      if self.div_regex.search(line):
        if start_index > -1:
          div_queue.append(index)
      if self.div_end_regex.search(line):
        if start_index > -1:
          # If we pop our start_index, we have found the matching </div>
          # to our <div class="tabs">
          if div_queue.pop() == start_index:
            start_end_pairs[start_index] = index
            start_index = -1

    # Delete the offending lines
    keys = start_end_pairs.keys()
    keys.sort()
    keys.reverse()
    for k in keys:
      del lines_list[k:start_end_pairs[k]+1]
    return lines_list

  def RenameFiles(self, input_file):
    """Fix file suffixes from .html to .ezt.

    This will also backup original html files to the originals directory.

    This operation will overwrite the previous ezt file.

    NOTE: If the originals directory is not a directory, the file will
    be overwritten.

    Args:
      input_file: the file to fix.
    """
    # If the file does not end in .html, we do not want to continue
    if self.html_suffix_regex.search(input_file):
      new_name = re.sub(self.html_suffix_regex, '.ezt', input_file)
      shutil.copy(input_file, self.originals_directory)
      # Rename file. If file exists, rename will not overwrite, and produce
      # OSError. So delete the old file first in that case.
      try:
        os.rename(input_file, new_name)
      except OSError:
        os.remove(new_name)
        os.rename(input_file, new_name)


def main():
  usage_string = ('This script is meant to convert .html files into .ezt files '
                  'in addition to eliminating any C++-style notations and '
                  'replacing them with their Javascript-style counter parts.\n'
                  'Note: This will write over existing files as well as nuke '
                  'any associated directories such as "original_html" located '
                  'in the same directory as the first argument\n'
                  'Usage: ezt_formatter.py <html_file(s)>')

  files = sys.argv[1:]
  if not files:
    # We have no files, so return, showing usage
    print usage_string
    return

  # current_directory should be the same for all files
  current_directory = os.path.dirname(files[0])
  formatter = EZTFormatter(current_directory)
  dj = DoxygenJavascriptifier()
  formatter.RegisterModifier(formatter.RemoveNavigation)
  formatter.RegisterModifier(dj.Javascriptify)
  formatter.RegisterModifier(formatter.FixBrackets)

  for f in files:
    formatter.PerformFunctionsOnFile(f)
    formatter.RenameFiles(f)

  formatter.MakeBackupCopy('stylesheet.css')

if __name__ == '__main__':
  main()
