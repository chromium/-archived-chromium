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

'''Tool to create a new, empty .grd file with all the basic sections.
'''

from grit.tool import interface
from grit import constants
from grit import util

# The contents of the new .grd file
_FILE_CONTENTS = '''\
<?xml version="1.0" encoding="UTF-8"?>
<grit base_dir="." latest_public_release="0" current_release="1"
      source_lang_id="en" enc_check="%s">
  <outputs>
    <!-- TODO add each of your output files.  Modify the three below, and add
    your own for your various languages.  See the user's guide for more
    details.
    Note that all output references are relative to the output directory
    which is specified at build time. -->
    <output filename="resource.h" type="rc_header" />
    <output filename="en_resource.rc" type="rc_all" />
    <output filename="fr_resource.rc" type="rc_all" />
  </outputs>
  <translations>
    <!-- TODO add references to each of the XTB files (from the Translation
    Console) that contain translations of messages in your project.  Each
    takes a form like <file path="english.xtb" />.  Remember that all file
    references are relative to this .grd file. -->
  </translations>
  <release seq="1">
    <includes>
      <!-- TODO add a list of your included resources here, e.g. BMP and GIF
      resources. -->
    </includes>
    <structures>
      <!-- TODO add a list of all your structured resources here, e.g. HTML
      templates, menus, dialogs etc.  Note that for menus, dialogs and version
      information resources you reference an .rc file containing them.-->
    </structures>
    <messages>
      <!-- TODO add all of your "string table" messages here.  Remember to
      change nontranslateable parts of the messages into placeholders (using the
      <ph> element).  You can also use the 'grit add' tool to help you identify
      nontranslateable parts and create placeholders for them. -->
    </messages>
  </release>
</grit>''' % constants.ENCODING_CHECK


class NewGrd(interface.Tool):
  '''Usage: grit newgrd OUTPUT_FILE

Creates a new, empty .grd file OUTPUT_FILE with comments about what to put
where in the file.'''

  def ShortDescription(self):
    return 'Create a new empty .grd file.'
  
  def Run(self, global_options, my_arguments):
    if not len(my_arguments) == 1:
      print 'This tool requires exactly one argument, the name of the output file.'
      return 2
    filename = my_arguments[0]
    out = util.WrapOutputStream(file(filename, 'w'), 'utf-8')
    out.write(_FILE_CONTENTS)
    out.close()
    print "Wrote file %s" % filename