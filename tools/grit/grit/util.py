#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Utilities used by GRIT.
'''

import sys
import os.path
import codecs
import htmlentitydefs
import re
import time
from xml.sax import saxutils

_root_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))


# Matches all of the resource IDs predefined by Windows.
# The '\b' before and after each word makes sure these match only whole words and
# not the beginning of any word.. eg. ID_FILE_NEW will not match ID_FILE_NEW_PROJECT
# see http://www.amk.ca/python/howto/regex/ (search for "\bclass\b" inside the html page)
SYSTEM_IDENTIFIERS = re.compile(
  r'''\bIDOK\b | \bIDCANCEL\b | \bIDC_STATIC\b | \bIDYES\b | \bIDNO\b |
      \bID_FILE_NEW\b | \bID_FILE_OPEN\b | \bID_FILE_CLOSE\b | \bID_FILE_SAVE\b |
      \bID_FILE_SAVE_AS\b | \bID_FILE_PAGE_SETUP\b | \bID_FILE_PRINT_SETUP\b |
      \bID_FILE_PRINT\b | \bID_FILE_PRINT_DIRECT\b | \bID_FILE_PRINT_PREVIEW\b |
      \bID_FILE_UPDATE\b | \bID_FILE_SAVE_COPY_AS\b | \bID_FILE_SEND_MAIL\b |
      \bID_FILE_MRU_FIRST\b | \bID_FILE_MRU_LAST\b |
      \bID_EDIT_CLEAR\b | \bID_EDIT_CLEAR_ALL\b | \bID_EDIT_COPY\b |
      \bID_EDIT_CUT\b | \bID_EDIT_FIND\b | \bID_EDIT_PASTE\b | \bID_EDIT_PASTE_LINK\b |
      \bID_EDIT_PASTE_SPECIAL\b | \bID_EDIT_REPEAT\b | \bID_EDIT_REPLACE\b |
      \bID_EDIT_SELECT_ALL\b | \bID_EDIT_UNDO\b | \bID_EDIT_REDO\b |
      \bVS_VERSION_INFO\b | \bIDRETRY''', re.VERBOSE);


# Matches character entities, whether specified by name, decimal or hex.
_HTML_ENTITY = re.compile(
  '&(#(?P<decimal>[0-9]+)|#x(?P<hex>[a-fA-F0-9]+)|(?P<named>[a-z0-9]+));',
  re.IGNORECASE)

# Matches characters that should be HTML-escaped.  This is <, > and &, but only
# if the & is not the start of an HTML character entity.
_HTML_CHARS_TO_ESCAPE = re.compile('"|<|>|&(?!#[0-9]+|#x[0-9a-z]+|[a-z]+;)',
                                   re.IGNORECASE | re.MULTILINE)


def WrapInputStream(stream, encoding = 'utf-8'):
  '''Returns a stream that wraps the provided stream, making it read characters
  using the specified encoding.'''
  (e, d, sr, sw) = codecs.lookup(encoding)
  return sr(stream)


def WrapOutputStream(stream, encoding = 'utf-8'):
  '''Returns a stream that wraps the provided stream, making it write
  characters using the specified encoding.'''
  (e, d, sr, sw) = codecs.lookup(encoding)
  return sw(stream)


def ChangeStdoutEncoding(encoding = 'utf-8'):
  '''Changes STDOUT to print characters using the specified encoding.'''
  sys.stdout = WrapOutputStream(sys.stdout, encoding)


def EscapeHtml(text, escape_quotes = False):
  '''Returns 'text' with <, > and & (and optionally ") escaped to named HTML
  entities.  Any existing named entity or HTML entity defined by decimal or
  hex code will be left untouched.  This is appropriate for escaping text for
  inclusion in HTML, but not for XML.
  '''
  def Replace(match):
    if match.group() == '&': return '&amp;'
    elif match.group() == '<': return '&lt;'
    elif match.group() == '>': return '&gt;'
    elif match.group() == '"':
      if escape_quotes: return '&quot;'
      else: return match.group()
    else: assert False
  out = _HTML_CHARS_TO_ESCAPE.sub(Replace, text)
  return out


def UnescapeHtml(text, replace_nbsp=True):
  '''Returns 'text' with all HTML character entities (both named character
  entities and those specified by decimal or hexadecimal Unicode ordinal)
  replaced by their Unicode characters (or latin1 characters if possible).

  The only exception is that &nbsp; will not be escaped if 'replace_nbsp' is
  False.
  '''
  def Replace(match):
    groups = match.groupdict()
    if groups['hex']:
      return unichr(int(groups['hex'], 16))
    elif groups['decimal']:
      return unichr(int(groups['decimal'], 10))
    else:
      name = groups['named']
      if name == 'nbsp' and not replace_nbsp:
        return match.group()  # Don't replace &nbsp;
      assert name != None
      if name in htmlentitydefs.name2codepoint.keys():
        return unichr(htmlentitydefs.name2codepoint[name])
      else:
        return match.group()  # Unknown HTML character entity - don't replace

  out = _HTML_ENTITY.sub(Replace, text)
  return out


def EncodeCdata(cdata):
  '''Returns the provided cdata in either escaped format or <![CDATA[xxx]]>
  format, depending on which is more appropriate for easy editing.  The data
  is escaped for inclusion in an XML element's body.

  Args:
    cdata: 'If x < y and y < z then x < z'

  Return:
    '<![CDATA[If x < y and y < z then x < z]]>'
  '''
  if cdata.count('<') > 1 or cdata.count('>') > 1 and cdata.count(']]>') == 0:
    return '<![CDATA[%s]]>' % cdata
  else:
    return saxutils.escape(cdata)


def FixupNamedParam(function, param_name, param_value):
  '''Returns a closure that is identical to 'function' but ensures that the
  named parameter 'param_name' is always set to 'param_value' unless explicitly
  set by the caller.

  Args:
    function: callable
    param_name: 'bingo'
    param_value: 'bongo' (any type)

  Return:
    callable
  '''
  def FixupClosure(*args, **kw):
    if not param_name in kw:
      kw[param_name] = param_value
    return function(*args, **kw)
  return FixupClosure


def PathFromRoot(path):
  '''Takes a path relative to the root directory for GRIT (the one that grit.py
  resides in) and returns a path that is either absolute or relative to the
  current working directory (i.e .a path you can use to open the file).

  Args:
    path: 'rel_dir\file.ext'

  Return:
    'c:\src\tools\rel_dir\file.ext
  '''
  return os.path.normpath(os.path.join(_root_dir, path))


def FixRootForUnittest(root_node, dir=PathFromRoot('.')):
  '''Adds a GetBaseDir() method to 'root_node', making unittesting easier.'''
  def GetBaseDir():
    '''Returns a fake base directory.'''
    return dir
  def GetSourceLanguage():
    return 'en'
  if not hasattr(root_node, 'GetBaseDir'):
    setattr(root_node, 'GetBaseDir', GetBaseDir)
    setattr(root_node, 'GetSourceLanguage', GetSourceLanguage)


def dirname(filename):
  '''Version of os.path.dirname() that never returns empty paths (returns
  '.' if the result of os.path.dirname() is empty).
  '''
  ret = os.path.dirname(filename)
  if ret == '':
    ret = '.'
  return ret


def normpath(path):
  '''Version of os.path.normpath that also changes backward slashes to
  forward slashes when not running on Windows.
  '''
  # This is safe to always do because the Windows version of os.path.normpath
  # will replace forward slashes with backward slashes.
  path = path.replace('\\', '/')
  return os.path.normpath(path)


_LANGUAGE_SPLIT_RE = re.compile('-|_|/')


def CanonicalLanguage(code):
  '''Canonicalizes two-part language codes by using a dash and making the
  second part upper case.  Returns one-part language codes unchanged.

  Args:
    code: 'zh_cn'

  Return:
    code: 'zh-CN'
  '''
  parts = _LANGUAGE_SPLIT_RE.split(code)
  code = [ parts[0] ]
  for part in parts[1:]:
    code.append(part.upper())
  return '-'.join(code)


_LANG_TO_CODEPAGE = {
  'en' : 1252,
  'fr' : 1252,
  'it' : 1252,
  'de' : 1252,
  'es' : 1252,
  'nl' : 1252,
  'sv' : 1252,
  'no' : 1252,
  'da' : 1252,
  'fi' : 1252,
  'pt-BR' : 1252,
  'ru' : 1251,
  'ja' : 932,
  'zh-TW' : 950,
  'zh-CN' : 936,
  'ko' : 949,
}


def LanguageToCodepage(lang):
  '''Returns the codepage _number_ that can be used to represent 'lang', which
  may be either in formats such as 'en', 'pt_br', 'pt-BR', etc.

  The codepage returned will be one of the 'cpXXXX' codepage numbers.

  Args:
    lang: 'de'

  Return:
    1252
  '''
  lang = CanonicalLanguage(lang)
  if lang in _LANG_TO_CODEPAGE:
    return _LANG_TO_CODEPAGE[lang]
  else:
    print "Not sure which codepage to use for %s, assuming cp1252" % lang
    return 1252

def NewClassInstance(class_name, class_type):
  '''Returns an instance of the class specified in classname

  Args:
    class_name: the fully qualified, dot separated package + classname,
    i.e. "my.package.name.MyClass". Short class names are not supported.
    class_type: the class or superclass this object must implement

  Return:
    An instance of the class, or None if none was found
  '''
  lastdot = class_name.rfind('.')
  module_name = ''
  if lastdot >= 0:
    module_name = class_name[0:lastdot]
    if module_name:
      class_name = class_name[lastdot+1:]
      module = __import__(module_name, globals(), locals(), [''])
      if hasattr(module, class_name):
        class_ = getattr(module, class_name)
        class_instance = class_()
        if isinstance(class_instance, class_type):
          return class_instance
  return None


def FixLineEnd(text, line_end):
  # First normalize
  text = text.replace('\r\n', '\n')
  text = text.replace('\r', '\n')
  # Then fix
  text = text.replace('\n', line_end)
  return text


def BoolToString(bool):
  if bool:
    return 'true'
  else:
    return 'false'


verbose = False
extra_verbose = False

def IsVerbose():
  return verbose

def IsExtraVerbose():
  return extra_verbose

def GetCurrentYear():
  '''Returns the current 4-digit year as an integer.'''
  return time.localtime()[0]

