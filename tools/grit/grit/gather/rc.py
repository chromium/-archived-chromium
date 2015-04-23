#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Support for gathering resources from RC files.
'''


import re
import types

from grit import clique
from grit import exception
from grit import util
from grit import tclib

from grit.gather import regexp


# Find portions that need unescaping in resource strings.  We need to be
# careful that a \\n is matched _first_ as a \\ rather than matching as
# a \ followed by a \n.
# TODO(joi) Handle ampersands if we decide to change them into <ph>
# TODO(joi) May need to handle other control characters than \n
_NEED_UNESCAPE = re.compile(r'""|\\\\|\\n|\\t')

# Find portions that need escaping to encode string as a resource string.
_NEED_ESCAPE = re.compile(r'"|\n|\t|\\|\&nbsp\;')

# How to escape certain characters
_ESCAPE_CHARS = {
  '"' : '""',
  '\n' : '\\n',
  '\t' : '\\t',
  '\\' : '\\\\',
  '&nbsp;' : ' '
}

# How to unescape certain strings
_UNESCAPE_CHARS = dict([[value, key] for key, value in _ESCAPE_CHARS.items()])



class Section(regexp.RegexpGatherer):
  '''A section from a resource file.'''

  def __init__(self, section_text):
    '''Creates a new object.

    Args:
      section_text: 'ID_SECTION_ID SECTIONTYPE\n.....\nBEGIN\n.....\nEND'
    '''
    regexp.RegexpGatherer.__init__(self, section_text)

  # static method
  def Escape(text):
    '''Returns a version of 'text' with characters escaped that need to be
    for inclusion in a resource section.'''
    def Replace(match):
      return _ESCAPE_CHARS[match.group()]
    return _NEED_ESCAPE.sub(Replace, text)
  Escape = staticmethod(Escape)

  # static method
  def UnEscape(text):
    '''Returns a version of 'text' with escaped characters unescaped.'''
    def Replace(match):
      return _UNESCAPE_CHARS[match.group()]
    return _NEED_UNESCAPE.sub(Replace, text)
  UnEscape = staticmethod(UnEscape)

  def _RegExpParse(self, rexp, text_to_parse):
    '''Overrides _RegExpParse to add shortcut group handling.  Otherwise
    the same.
    '''
    regexp.RegexpGatherer._RegExpParse(self, rexp, text_to_parse)

    if not self.IsSkeleton() and len(self.GetTextualIds()) > 0:
      group_name = self.GetTextualIds()[0]
      for c in self.GetCliques():
        c.AddToShortcutGroup(group_name)

  # Static method
  def FromFileImpl(rc_file, extkey, encoding, type):
    '''Implementation of FromFile.  Need to keep separate so we can have
    a FromFile in this class that has its type set to Section by default.
    '''
    if isinstance(rc_file, types.StringTypes):
      rc_file = util.WrapInputStream(file(rc_file, 'r'), encoding)

    out = ''
    begin_count = 0
    for line in rc_file.readlines():
      if len(out) > 0 or (line.strip().startswith(extkey) and
                          line.strip().split()[0] == extkey):
        out += line

      # we stop once we reach the END for the outermost block.
      begin_count_was = begin_count
      if len(out) > 0 and line.strip() == 'BEGIN':
        begin_count += 1
      elif len(out) > 0 and line.strip() == 'END':
        begin_count -= 1
      if begin_count_was == 1 and begin_count == 0:
        break

    if len(out) == 0:
      raise exception.SectionNotFound('%s in file %s' % (extkey, rc_file))

    return type(out)
  FromFileImpl = staticmethod(FromFileImpl)

  # static method
  def FromFile(rc_file, extkey, encoding='cp1252'):
    '''Retrieves the section of 'rc_file' that has the key 'extkey'.  This is
    matched against the start of a line, and that line and the rest of that
    section in the RC file is returned.

    If 'rc_file' is a filename, it will be opened for reading using 'encoding'.
    Otherwise the 'encoding' parameter is ignored.

    This method instantiates an object of type 'type' with the text from the
    file.

    Args:
      rc_file: file('') | 'filename.rc'
      extkey: 'ID_MY_DIALOG'
      encoding: 'utf-8'
      type: class to instantiate with text of section

    Return:
      type(text_of_section)
    '''
    return Section.FromFileImpl(rc_file, extkey, encoding, Section)
  FromFile = staticmethod(FromFile)


class Dialog(Section):
  '''A resource section that contains a dialog resource.'''

  # A typical dialog resource section looks like this:
  #
  # IDD_ABOUTBOX DIALOGEX 22, 17, 230, 75
  # STYLE DS_SETFONT | DS_MODALFRAME | WS_CAPTION | WS_SYSMENU
  # CAPTION "About"
  # FONT 8, "System", 0, 0, 0x0
  # BEGIN
  #     ICON            IDI_KLONK,IDC_MYICON,14,9,20,20
  #     LTEXT           "klonk Version ""yibbee"" 1.0",IDC_STATIC,49,10,119,8,
  #                     SS_NOPREFIX
  #     LTEXT           "Copyright (C) 2005",IDC_STATIC,49,20,119,8
  #     DEFPUSHBUTTON   "OK",IDOK,195,6,30,11,WS_GROUP
  #     CONTROL         "Jack ""Black"" Daniels",IDC_RADIO1,"Button",
  #                     BS_AUTORADIOBUTTON,46,51,84,10
  # END

  # We are using a sorted set of keys, and we assume that the
  # group name used for descriptions (type) will come after the "text"
  # group in alphabetical order. We also assume that there cannot be
  # more than one description per regular expression match.
  # If that's not the case some descriptions will be clobbered.
  dialog_re_ = re.compile('''
    # The dialog's ID in the first line
    (?P<id1>[A-Z0-9_]+)\s+DIALOG(EX)?
    |
    # The caption of the dialog
    (?P<type1>CAPTION)\s+"(?P<text1>.*?([^"]|""))"\s
    |
    # Lines for controls that have text and an ID
    \s+(?P<type2>[A-Z]+)\s+"(?P<text2>.*?([^"]|"")?)"\s*,\s*(?P<id2>[A-Z0-9_]+)\s*,
    |
    # Lines for controls that have text only
    \s+(?P<type3>[A-Z]+)\s+"(?P<text3>.*?([^"]|"")?)"\s*,
    |
    # Lines for controls that reference other resources
    \s+[A-Z]+\s+[A-Z0-9_]+\s*,\s*(?P<id3>[A-Z0-9_]*[A-Z][A-Z0-9_]*)
    |
    # This matches "NOT SOME_STYLE" so that it gets consumed and doesn't get
    # matched by the next option (controls that have only an ID and then just
    # numbers)
    \s+NOT\s+[A-Z][A-Z0-9_]+
    |
    # Lines for controls that have only an ID and then just numbers
    \s+[A-Z]+\s+(?P<id4>[A-Z0-9_]*[A-Z][A-Z0-9_]*)\s*,
    ''', re.MULTILINE | re.VERBOSE)

  def Parse(self):
    '''Knows how to parse dialog resource sections.'''
    self._RegExpParse(self.dialog_re_, self.text_)

  # static method
  def FromFile(rc_file, extkey, encoding = 'cp1252'):
    return Section.FromFileImpl(rc_file, extkey, encoding, Dialog)
  FromFile = staticmethod(FromFile)


class Menu(Section):
  '''A resource section that contains a menu resource.'''

  # A typical menu resource section looks something like this:
  #
  # IDC_KLONK MENU
  # BEGIN
  #     POPUP "&File"
  #     BEGIN
  #         MENUITEM "E&xit",                       IDM_EXIT
  #         MENUITEM "This be ""Klonk"" me like",   ID_FILE_THISBE
  #         POPUP "gonk"
  #         BEGIN
  #             MENUITEM "Klonk && is ""good""",           ID_GONK_KLONKIS
  #         END
  #     END
  #     POPUP "&Help"
  #     BEGIN
  #         MENUITEM "&About ...",                  IDM_ABOUT
  #     END
  # END

  # Description used for the messages generated for menus, to explain to
  # the translators how to handle them.
  MENU_MESSAGE_DESCRIPTION = (
    'This message represents a menu. Each of the items appears in sequence '
    '(some possibly within sub-menus) in the menu. The XX01XX placeholders '
    'serve to separate items. Each item contains an & (ampersand) character '
    'in front of the keystroke that should be used as a shortcut for that item '
    'in the menu. Please make sure that no two items in the same menu share '
    'the same shortcut.'
  )

  # A dandy regexp to suck all the IDs and translateables out of a menu
  # resource
  menu_re_ = re.compile('''
    # Match the MENU ID on the first line
    ^(?P<id1>[A-Z0-9_]+)\s+MENU
    |
    # Match the translateable caption for a popup menu
    POPUP\s+"(?P<text1>.*?([^"]|""))"\s
    |
    # Match the caption & ID of a MENUITEM
    MENUITEM\s+"(?P<text2>.*?([^"]|""))"\s*,\s*(?P<id2>[A-Z0-9_]+)
    ''', re.MULTILINE | re.VERBOSE)

  def Parse(self):
    '''Knows how to parse menu resource sections.  Because it is important that
    menu shortcuts are unique within the menu, we return each menu as a single
    message with placeholders to break up the different menu items, rather than
    return a single message per menu item.  we also add an automatic description
    with instructions for the translators.'''
    self.single_message_ = tclib.Message(description=self.MENU_MESSAGE_DESCRIPTION)
    self._RegExpParse(self.menu_re_, self.text_)

  # static method
  def FromFile(rc_file, extkey, encoding = 'cp1252'):
    return Section.FromFileImpl(rc_file, extkey, encoding, Menu)
  FromFile = staticmethod(FromFile)


class Version(Section):
  '''A resource section that contains a VERSIONINFO resource.'''

  # A typical version info resource can look like this:
  #
  # VS_VERSION_INFO VERSIONINFO
  #  FILEVERSION 1,0,0,1
  #  PRODUCTVERSION 1,0,0,1
  #  FILEFLAGSMASK 0x3fL
  # #ifdef _DEBUG
  #  FILEFLAGS 0x1L
  # #else
  #  FILEFLAGS 0x0L
  # #endif
  #  FILEOS 0x4L
  #  FILETYPE 0x2L
  #  FILESUBTYPE 0x0L
  # BEGIN
  #     BLOCK "StringFileInfo"
  #     BEGIN
  #         BLOCK "040904e4"
  #         BEGIN
  #             VALUE "CompanyName", "TODO: <Company name>"
  #             VALUE "FileDescription", "TODO: <File description>"
  #             VALUE "FileVersion", "1.0.0.1"
  #             VALUE "LegalCopyright", "TODO: (c) <Company name>.  All rights reserved."
  #             VALUE "InternalName", "res_format_test.dll"
  #             VALUE "OriginalFilename", "res_format_test.dll"
  #             VALUE "ProductName", "TODO: <Product name>"
  #             VALUE "ProductVersion", "1.0.0.1"
  #         END
  #     END
  #     BLOCK "VarFileInfo"
  #     BEGIN
  #         VALUE "Translation", 0x409, 1252
  #     END
  # END
  #
  #
  # In addition to the above fields, VALUE fields named "Comments" and
  # "LegalTrademarks" may also be translateable.

  version_re_ = re.compile('''
    # Match the ID on the first line
    ^(?P<id1>[A-Z0-9_]+)\s+VERSIONINFO
    |
    # Match all potentially translateable VALUE sections
    \s+VALUE\s+"
    (
      CompanyName|FileDescription|LegalCopyright|
      ProductName|Comments|LegalTrademarks
    )",\s+"(?P<text1>.*?([^"]|""))"\s
    ''', re.MULTILINE | re.VERBOSE)

  def Parse(self):
    '''Knows how to parse VERSIONINFO resource sections.'''
    self._RegExpParse(self.version_re_, self.text_)

  # TODO(joi) May need to override the Translate() method to change the
  # "Translation" VALUE block to indicate the correct language code.

  # static method
  def FromFile(rc_file, extkey, encoding = 'cp1252'):
    return Section.FromFileImpl(rc_file, extkey, encoding, Version)
  FromFile = staticmethod(FromFile)

class RCData(Section):
  '''A resource section that contains some data .'''

  # A typical rcdataresource section looks like this:
  #
  # IDR_BLAH        RCDATA      { 1, 2, 3, 4 }

  dialog_re_ = re.compile('''
    ^(?P<id1>[A-Z0-9_]+)\s+RCDATA\s+(DISCARDABLE)?\s+\{.*?\}
    ''', re.MULTILINE | re.VERBOSE | re.DOTALL)

  def Parse(self):
    '''Knows how to parse RCDATA resource sections.'''
    self._RegExpParse(self.dialog_re_, self.text_)

  # static method
  def FromFile(rc_file, extkey, encoding = 'cp1252'):
    '''Implementation of FromFile for resource types w/braces (not BEGIN/END)
    '''
    if isinstance(rc_file, types.StringTypes):
      rc_file = util.WrapInputStream(file(rc_file, 'r'), encoding)

    out = ''
    begin_count = 0
    openbrace_count = 0
    for line in rc_file.readlines():
      if len(out) > 0 or line.strip().startswith(extkey):
        out += line

      # we stop once balance the braces (could happen on one line)
      begin_count_was = begin_count
      if len(out) > 0:
        openbrace_count += line.count('{')
        begin_count += line.count('{')
        begin_count -= line.count('}')
      if ((begin_count_was == 1 and begin_count == 0) or
         (openbrace_count > 0 and begin_count == 0)):
        break

    if len(out) == 0:
      raise exception.SectionNotFound('%s in file %s' % (extkey, rc_file))

    return RCData(out)
  FromFile = staticmethod(FromFile)


class Accelerators(Section):
  '''An ACCELERATORS table.
  '''

  # A typical ACCELERATORS section looks like this:
  #
  # IDR_ACCELERATOR1 ACCELERATORS
  # BEGIN
  #   "^C",           ID_ACCELERATOR32770,    ASCII,  NOINVERT
  #   "^V",           ID_ACCELERATOR32771,    ASCII,  NOINVERT
  #   VK_INSERT,      ID_ACCELERATOR32772,    VIRTKEY, CONTROL, NOINVERT
  # END

  accelerators_re_ = re.compile('''
    # Match the ID on the first line
    ^(?P<id1>[A-Z0-9_]+)\s+ACCELERATORS\s+
    |
    # Match accelerators specified as VK_XXX
    \s+VK_[A-Z0-9_]+,\s*(?P<id2>[A-Z0-9_]+)\s*,
    |
    # Match accelerators specified as e.g. "^C"
    \s+"[^"]*",\s+(?P<id3>[A-Z0-9_]+)\s*,
    ''', re.MULTILINE | re.VERBOSE)

  def Parse(self):
    '''Knows how to parse ACCELERATORS resource sections.'''
    self._RegExpParse(self.accelerators_re_, self.text_)

  # static method
  def FromFile(rc_file, extkey, encoding = 'cp1252'):
    return Section.FromFileImpl(rc_file, extkey, encoding, Accelerators)
  FromFile = staticmethod(FromFile)

