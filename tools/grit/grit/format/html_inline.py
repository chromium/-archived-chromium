#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Flattens a HTML file by inlining its external resources.

This is a small script that takes a HTML file, looks for src attributes
and inlines the specified file, producing one HTML file with no external
dependencies.

This does not inline CSS styles, nor does it inline anything referenced
from an inlined file.
"""

import os
import re
import sys
import base64
import mimetypes

DIST_DEFAULT = 'chromium'
DIST_ENV_VAR = 'CHROMIUM_BUILD'
DIST_SUBSTR = '%DISTRIBUTION%'

def ReadFile(input_filename):
  """Helper function that returns input_filename as a string.
  
  Args:
    input_filename: name of file to be read
  
  Returns:
    string
  """
  f = open(input_filename, 'rb')
  file_contents = f.read()
  f.close()
  return file_contents

def SrcInline(src_match, base_path, distribution):
  """regex replace function.

  Takes a regex match for src="filename", attempts to read the file 
  at 'filename' and returns the src attribute with the file inlined
  as a data URI. If it finds DIST_SUBSTR string in file name, replaces
  it with distribution.

  Args:
    src_match: regex match object with 'filename' named capturing group
    base_path: path that to look for files in
    distribution: string that should replace DIST_SUBSTR

  Returns:
    string
  """
  filename = src_match.group('filename')

  if filename.find(':') != -1:
    # filename is probably a URL, which we don't want to bother inlining
    return src_match.group(0)

  filename = filename.replace('%DISTRIBUTION%', distribution)
  filepath = os.path.join(base_path, filename)    
  mimetype = mimetypes.guess_type(filename)[0] or 'text/plain'
  inline_data = base64.standard_b64encode(ReadFile(filepath))

  prefix = src_match.string[src_match.start():src_match.start('filename')-1]
  return "%s\"data:%s;base64,%s\"" % (prefix, mimetype, inline_data)
  
def InlineFile(input_filename, output_filename):
  """Inlines the resources in a specified file.
  
  Reads input_filename, finds all the src attributes and attempts to
  inline the files they are referring to, then writes the result
  to output_filename.
  
  Args:
    input_filename: name of file to read in
    output_filename: name of file to be written to
  """
  print "inlining %s to %s" % (input_filename, output_filename)
  input_filepath = os.path.dirname(input_filename)  
 
  distribution = DIST_DEFAULT
  if DIST_ENV_VAR in os.environ.keys():
    distribution = os.environ[DIST_ENV_VAR]
    if len(distribution) > 1 and distribution[0] == '_':
      distribution = distribution[1:].lower()
      
  def SrcReplace(src_match):
    """Helper function to provide SrcInline with the base file path"""
    return SrcInline(src_match, input_filepath, distribution)
 
  # TODO(glen): Make this regex not match src="" text that is not inside a tag
  flat_text = re.sub('src="(?P<filename>[^"\']*)"',
                     SrcReplace,
                     ReadFile(input_filename))

  # TODO(glen): Make this regex not match url('') that is not inside a style
  flat_text = re.sub('background:[ ]*url\(\'(?P<filename>[^"\']*)\'',
                    SrcReplace,
                    flat_text)

  out_file = open(output_filename, 'wb')
  out_file.writelines(flat_text)
  out_file.close()

def main():
  if len(sys.argv) <= 2:
    print "Flattens a HTML file by inlining its external resources.\n"
    print "html_inline.py inputfile outputfile"
  else:
    InlineFile(sys.argv[1], sys.argv[2])

if __name__ == '__main__':
  main()

