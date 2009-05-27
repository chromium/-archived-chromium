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


"""Creates unordered list of classes and packages."""


import os.path
import re
import sys

# Special regular expressions for search and replace
list_item_string = ('<a class="el" href="([a-zA-Z0-9_]*.html)">'
                    '([a-zA-Z0-9\.]*)</a></td>')
LIST_ITEM = re.compile(list_item_string)


def HyphenateWord(original_word, max_length, join_str='-'):
  """Breaks a word at max_length with hyphens.

  If the word is still too long (i.e., length > 2*max_length), the word will
  be split again. The word will be split at word breaks defined by a
  switch from lowercase to uppercase. The function will attempt to break at
  the closest switch before the max_length number of letters. If there is no
  upper case letter before the max_length, the original word will be returned.
  If max_length is 0 the original word will be returned.

  Args:
    original_word: the word to break.
    max_length: maximum length in chars for any piece of the word.
    join_str: characters to join the hyphenated parts with, defaults to '-'
  Returns:
    the hyphenated word.
  """
  word_list = []
  while len(original_word) > max_length:
    for i in range(max_length, -1, -1):
      if original_word[i].isupper():
        word_list += [original_word[:i]]
        original_word = original_word[i:]
        break
      if i == 0:
        # There was no uppercase letter found so we don't break and just
        # return the original word
        return original_word

  word_list += [original_word]

  return join_str.join(word_list)


def CreateListItems(input_html_file, max_length, html_directory):
  """Creates html for a list of items based on the input html file.

  Args:
    input_html_file: file to extract list items from.
    max_length: maximum length for sidebar entry.
    html_directory: relative html path for linked files.
  Returns:
    item_list: a list representing the new html.
  """
  infile = open(input_html_file, 'r')
  lines_list = infile.readlines()
  infile.close()

  item_list = ['    <ul>\n']
  format_string = ('      <li><a href="%s%s">%s</a></li>\n')
  heading_format_string = ('      <li><font class="sidebar-heading">%s'
                           '</font></li>\n')

  previous_package = ''
  for line in lines_list:
    match = re.search(LIST_ITEM, line)
    if match:
      split_name = match.group(2).split('.')
      short_name = HyphenateWord(split_name[-1], max_length, '-<br>')
      package = '.'.join(split_name[:-1])
      if package != previous_package:
        item_list += [heading_format_string % package]
        previous_package = package
      item_list += [format_string % (html_directory, match.group(1),
                                     short_name)]

  item_list += ['    </ul>\n']

  return item_list


def CreateClassTreeHtmlFile(classtree_items, directory, html_directory):
  """Creates classtree.html file which has item lists based on class structure.

  The html file is only the lists and no surrounding html (body tag, etc.) as
  it is meant to be inserted into ezt html.

  Args:
    classtree_items: list of header-filename pairs to create list from.
    directory: location of the input html files to CreateListItems.
    html_directory: relative html path for linked files.
  """
  # Open file to write
  filename = os.path.join(directory, 'classtree.html')
  outfile = open(filename, 'w')

  originals_directory = os.path.join(directory, 'original_html')

  output_list = ['<ul>\n']
  for ct_item in classtree_items:
    output_list += ['  <li><a href="%s' % html_directory]
    output_list += ['%s">%s</a>\n' % (ct_item[1], ct_item[0])]
    full_file_name = os.path.join(originals_directory, ct_item[1])
    output_list += CreateListItems(full_file_name, 16, html_directory)
    output_list += ['  </li>\n']
  output_list += ['</ul>\n']

  outfile.writelines(output_list)
  outfile.close()


def main():
  usage_string = 'This script  generates an html list of the classes and '
  usage_string += 'packages. The output file is called classtree.html. If no '
  usage_string += 'directory is specified, . is used as a default\n'
  usage_string += 'Usage: classtree.py <directory>'

  print usage_string

  # Specify the default directory
  directory = '.'
  if len(sys.argv) > 1:
    directory = sys.argv[1]
  #create classtree items
  classtree_items = [('Packages', 'namespaces.html'),
                     ('Classes', 'annotated.html')]
  # create the class tree
  CreateClassTreeHtmlFile(classtree_items, directory,
                          '/apis/o3d/docs/reference/')

if __name__ == '__main__':
  main()
