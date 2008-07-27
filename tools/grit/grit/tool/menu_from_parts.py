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

'''The 'grit menufromparts' tool.'''

import os
import getopt
import types

from grit.tool import interface
from grit.tool import transl2tc
from grit import grd_reader
from grit import tclib
from grit import util
from grit import xtb_reader


import grit.extern.tclib


class MenuTranslationsFromParts(interface.Tool):
  '''One-off tool to generate translated menu messages (where each menu is kept
in a single message) based on existing translations of the individual menu
items.  Was needed when changing menus from being one message per menu item
to being one message for the whole menu.'''

  def ShortDescription(self):
    return ('Create translations of whole menus from existing translations of '
            'menu items.')
  
  def Run(self, globopt, args):
    self.SetOptions(globopt)
    assert len(args) == 2, "Need exactly two arguments, the XTB file and the output file"
    
    xtb_file = args[0]
    output_file = args[1]
    
    grd = grd_reader.Parse(self.o.input, debug=self.o.extra_verbose)
    grd.OnlyTheseTranslations([])  # don't load translations
    grd.RunGatherers(recursive = True)
    
    xtb = {}
    def Callback(msg_id, parts):
      msg = []
      for part in parts:
        if part[0]:
          msg = []
          break  # it had a placeholder so ignore it
        else:
          msg.append(part[1])
      if len(msg):
        xtb[msg_id] = ''.join(msg)
    f = file(xtb_file)
    xtb_reader.Parse(f, Callback)
    f.close()
    
    translations = []  # list of translations as per transl2tc.WriteTranslations
    for node in grd:
      if node.name == 'structure' and node.attrs['type'] == 'menu':
        assert len(node.GetCliques()) == 1
        message = node.GetCliques()[0].GetMessage()
        translation = []
        
        contents = message.GetContent()
        for part in contents:
          if isinstance(part, types.StringTypes):
            id = grit.extern.tclib.GenerateMessageId(part)
            if id not in xtb:
              print "WARNING didn't find all translations for menu %s" % node.attrs['name']
              translation = []
              break
            translation.append(xtb[id])
          else:
            translation.append(part.GetPresentation())
        
        if len(translation):
          translations.append([message.GetId(), ''.join(translation)])
    
    f = util.WrapOutputStream(file(output_file, 'w'))
    transl2tc.TranslationToTc.WriteTranslations(f, translations)
    f.close()
