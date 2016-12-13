#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

