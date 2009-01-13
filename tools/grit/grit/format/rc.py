#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Support for formatting an RC file for compilation.
'''

import os
import types
import re

from grit import util
from grit.format import interface


# Matches all different types of linebreaks.
_LINEBREAKS = re.compile('\r\n|\n|\r')

'''
This dictionary defines the langauge charset pair lookup table, which is used
for replacing the GRIT expand variables for language info in Product Version 
resource. The key is the language ISO country code, and the value 
is the language and character-set pair, which is a hexadecimal string
consisting of the concatenation of the language and character-set identifiers.
The first 4 digit of the value is the hex value of LCID, the remaining 
4 digits is the hex value of character-set id(code page)of the language.
 
We have defined three GRIT expand_variables to be used in the version resource
file to set the language info. Here is an example how they should be used in 
the VS_VERSION_INFO section of the resource file to allow GRIT to localize 
the language info correctly according to product locale.

VS_VERSION_INFO VERSIONINFO
... 
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
      BLOCK "[GRITVERLANGCHARSETHEX]"
        BEGIN
        ...
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", [GRITVERLANGID], [GRITVERCHARSETID]
    END
END

'''

_LANGUAGE_CHARSET_PAIR = {
  'ar'    : '040104e8',
  'fi'    : '040b04e4',
  'ko'    : '041203b5',
  'es'    : '040a04e4',
  'bg'    : '040204e3',
  'fr'    : '040c04e4',
  'lv'    : '042604e9',
  'sv'    : '041d04e4',
  'ca'    : '040304e4',
  'de'    : '040704e4',
  'lt'    : '042704e9',
  'tl'    : '0c0004b0',   # no lcid for tl(Tagalog), use default custom locale
  'zh-CN' : '080403a8',
  'el'    : '040804e5',
  'no'    : '041404e4',
  'th'    : '041e036a',
  'zh-TW' : '040403b6',
  'iw'    : '040d04e7',
  'pl'    : '041504e2',
  'tr'    : '041f04e6',
  'hr'    : '041a04e4',
  'hi'    : '043904b0',   # no codepage for hindi, use unicode(1200)
  'pt-BR' : '041604e4',
  'uk'    : '042204e3',
  'cs'    : '040504e2',
  'hu'    : '040e04e2',
  'ro'    : '041804e2',
  'ur'    : '042004b0',   # no codepage for urdu, use unicode(1200)
  'da'    : '040604e4',
  'is'    : '040f04e4',
  'ru'    : '041904e3',
  'vi'    : '042a04ea',
  'nl'    : '041304e4',
  'id'    : '042104e4',
  'sr'    : '081a04e2',
  'en-GB' : '0809040e',
  'it'    : '041004e4',
  'sk'    : '041b04e2',
  'et'    : '042504e9',
  'ja'    : '041103a4',
  'sl'    : '042404e2',  
  'en'    : '040904b0',
}

_LANGUAGE_DIRECTIVE_PAIR = {
  'ar'    : 'LANG_ARABIC, SUBLANG_DEFAULT',
  'fi'    : 'LANG_FINNISH, SUBLANG_DEFAULT',
  'ko'    : 'LANG_KOREAN, SUBLANG_KOREAN',
  'es'    : 'LANG_SPANISH, SUBLANG_SPANISH_MODERN',
  'bg'    : 'LANG_BULGARIAN, SUBLANG_DEFAULT',
  'fr'    : 'LANG_FRENCH, SUBLANG_FRENCH',
  'lv'    : 'LANG_LATVIAN, SUBLANG_DEFAULT',
  'sv'    : 'LANG_SWEDISH, SUBLANG_SWEDISH',
  'ca'    : 'LANG_CATALAN, SUBLANG_DEFAULT',
  'de'    : 'LANG_GERMAN, SUBLANG_GERMAN',
  'lt'    : 'LANG_LITHUANIAN, SUBLANG_LITHUANIAN',
  'tl'    : 'LANG_NEUTRAL, SUBLANG_DEFAULT',
  'zh-CN' : 'LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED',
  'el'    : 'LANG_GREEK, SUBLANG_DEFAULT',
  'no'    : 'LANG_NORWEGIAN, SUBLANG_DEFAULT',
  'th'    : 'LANG_THAI, SUBLANG_DEFAULT',
  'zh-TW' : 'LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL',
  'iw'    : 'LANG_HEBREW, SUBLANG_DEFAULT',
  'pl'    : 'LANG_POLISH, SUBLANG_DEFAULT',
  'tr'    : 'LANG_TURKISH, SUBLANG_DEFAULT',
  'hr'    : 'LANG_CROATIAN, SUBLANG_DEFAULT',
  'hi'    : 'LANG_HINDI, SUBLANG_DEFAULT',
  'pt-BR' : 'LANG_PORTUGUESE, SUBLANG_DEFAULT',
  'uk'    : 'LANG_UKRAINIAN, SUBLANG_DEFAULT',
  'cs'    : 'LANG_CZECH, SUBLANG_DEFAULT',
  'hu'    : 'LANG_HUNGARIAN, SUBLANG_DEFAULT',
  'ro'    : 'LANG_ROMANIAN, SUBLANG_DEFAULT',
  'ur'    : 'LANG_URDU, SUBLANG_DEFAULT',
  'da'    : 'LANG_DANISH, SUBLANG_DEFAULT',
  'is'    : 'LANG_ICELANDIC, SUBLANG_DEFAULT',
  'ru'    : 'LANG_RUSSIAN, SUBLANG_DEFAULT',
  'vi'    : 'LANG_VIETNAMESE, SUBLANG_DEFAULT',
  'nl'    : 'LANG_DUTCH, SUBLANG_DEFAULT',
  'id'    : 'LANG_INDONESIAN, SUBLANG_DEFAULT',
  'sr'    : 'LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC',
  'en-GB' : 'LANG_ENGLISH, SUBLANG_ENGLISH_UK',
  'it'    : 'LANG_ITALIAN, SUBLANG_DEFAULT',
  'sk'    : 'LANG_SLOVAK, SUBLANG_DEFAULT',
  'et'    : 'LANG_ESTONIAN, SUBLANG_DEFAULT',
  'ja'    : 'LANG_JAPANESE, SUBLANG_DEFAULT',
  'sl'    : 'LANG_SLOVENIAN, SUBLANG_DEFAULT',  
  'en'    : 'LANG_ENGLISH, SUBLANG_ENGLISH_US',
}

def GetLangCharsetPair(language) :  
  if _LANGUAGE_CHARSET_PAIR.has_key(language) :
    return _LANGUAGE_CHARSET_PAIR[language]
  else :
    print 'Warning:GetLangCharsetPair() found undefined language %s' %(language)
    return ''

def GetLangDirectivePair(language) :  
  if _LANGUAGE_DIRECTIVE_PAIR.has_key(language) :
    return _LANGUAGE_DIRECTIVE_PAIR[language]
  else :
    print 'Warning:GetLangDirectivePair() found undefined language %s' %(language)
    return 'unknown language: see tools/grit/format/rc.py'

def GetLangIdHex(language) :  
  if _LANGUAGE_CHARSET_PAIR.has_key(language) :
    langcharset = _LANGUAGE_CHARSET_PAIR[language]  
    lang_id = '0x' + langcharset[0:4]  
    return lang_id
  else :
    print 'Warning:GetLangIdHex() found undefined language %s' %(language)
    return ''


def GetCharsetIdDecimal(language) :
  if _LANGUAGE_CHARSET_PAIR.has_key(language) :
    langcharset = _LANGUAGE_CHARSET_PAIR[language]  
    charset_decimal = int(langcharset[4:], 16)  
    return str(charset_decimal)
  else :
    print 'Warning:GetCharsetIdDecimal() found undefined language %s' %(language)
    return ''


def GetUnifiedLangCode(language) :
  r = re.compile('([a-z]{1,2})_([a-z]{1,2})')
  if r.match(language) :
    underscore = language.find('_')
    return language[0:underscore] + '-' + language[underscore + 1:].upper()
  else :
    return language

    
def _MakeRelativePath(base_path, path_to_make_relative):
  '''Returns a relative path such from the base_path to
  the path_to_make_relative.
  
  In other words, os.join(base_path,
    MakeRelativePath(base_path, path_to_make_relative))
  is the same location as path_to_make_relative.
      
  Args:
    base_path: the root path
    path_to_make_relative: an absolute path that is on the same drive
      as base_path
  '''

  def _GetPathAfterPrefix(prefix_path, path_with_prefix):
    '''Gets the subpath within in prefix_path for the path_with_prefix
    with no beginning or trailing path separators.
      
    Args:
      prefix_path: the base path
      path_with_prefix: a path that starts with prefix_path
    '''
    assert path_with_prefix.startswith(prefix_path)
    path_without_prefix = path_with_prefix[len(prefix_path):]
    normalized_path = os.path.normpath(path_without_prefix.strip(os.path.sep))
    if normalized_path == '.':
      normalized_path = ''
    return normalized_path
  
  def _GetCommonBaseDirectory(*args):
    '''Returns the common prefix directory for the given paths

    Args:
      The list of paths (at least one of which should be a directory)
    '''
    prefix = os.path.commonprefix(args)
    # prefix is a character-by-character prefix (i.e. it does not end
    # on a directory bound, so this code fixes that)

    # if the prefix ends with the separator, then it is prefect.
    if len(prefix) > 0 and prefix[-1] == os.path.sep:
      return prefix

    # We need to loop through all paths or else we can get
    # tripped up by "c:\a" and "c:\abc".  The common prefix
    # is "c:\a" which is a directory and looks good with
    # respect to the first directory but it is clear that
    # isn't a common directory when the second path is
    # examined.
    for path in args:
      assert len(path) >= len(prefix)
      # If the prefix the same length as the path,
      # then the prefix must be a directory (since one
      # of the arguements should be a directory).
      if path == prefix:
        continue
      # if the character after the prefix in the path
      # is the separator, then the prefix appears to be a
      # valid a directory as well for the given path
      if path[len(prefix)] == os.path.sep:
        continue
      # Otherwise, the prefix is not a directory, so it needs
      # to be shortened to be one
      index_sep = prefix.rfind(os.path.sep)
      # The use "index_sep + 1" because it includes the final sep
      # and it handles the case when the index_sep is -1 as well
      prefix = prefix[:index_sep + 1]
      # At this point we backed up to a directory bound which is
      # common to all paths, so we can quit going through all of
      # the paths.
      break
    return prefix          

  prefix =  _GetCommonBaseDirectory(base_path, path_to_make_relative)
  # If the paths had no commonality at all, then return the absolute path
  # because it is the best that can be done.  If the path had to be relative
  # then eventually this absolute path will be discovered (when a build breaks)
  # and an appropriate fix can be made, but having this allows for the best 
  # backward compatibility with the absolute path behavior in the past.
  if len(prefix) <= 0:
    return path_to_make_relative
  # Build a path from the base dir to the common prefix
  remaining_base_path = _GetPathAfterPrefix(prefix, base_path)

  #  The follow handles two case: "" and "foo\\bar"
  path_pieces = remaining_base_path.split(os.path.sep)
  base_depth_from_prefix = len([d for d in path_pieces if len(d)])
  base_to_prefix = (".." + os.path.sep) * base_depth_from_prefix
  
  # Put add in the path from the prefix to the path_to_make_relative
  remaining_other_path = _GetPathAfterPrefix(prefix, path_to_make_relative)
  return base_to_prefix + remaining_other_path


class TopLevel(interface.ItemFormatter):
  '''Writes out the required preamble for RC files.'''
  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    assert isinstance(lang, types.StringTypes)
    if not begin_item:
      return ''
    else:
      # Find the location of the resource header file, so that we can include
      # it.
      resource_header = 'resource.h'  # fall back to this
      language_directive = ''
      for child in item.GetRoot().children:
        if child.name == 'outputs':
          for output in child.children:
            if output.attrs['type'] == 'rc_header':
              resource_header = os.path.abspath(output.GetOutputFilename())
              resource_header = _MakeRelativePath(output_dir, resource_header)
            if output.attrs['lang'] != lang:
              continue
            if output.attrs['language_section'] == '':
              # If no language_section is requested, no directive is added
              # (Used when the generated rc will be included from another rc 
              # file that will have the appropriate language directive)
              language_directive = ''
            elif output.attrs['language_section'] == 'neutral':
              # If a neutral language section is requested (default), add a 
              # neutral language directive
              language_directive = 'LANGUAGE LANG_NEUTRAL, SUBLANG_NEUTRAL'
            elif output.attrs['language_section'] == 'lang':
              language_directive = 'LANGUAGE %s' % GetLangDirectivePair(lang)
      resource_header = resource_header.replace('\\', '\\\\')
      return '''// Copyright (c) Google Inc. %d
// All rights reserved.
// This file is automatically generated by GRIT.  Do not edit.

#include "%s"
#include <winres.h>
#include <winresrc.h>

%s


''' % (util.GetCurrentYear(), resource_header, language_directive)
# end Format() function



class StringTable(interface.ItemFormatter):
  '''Surrounds a collection of string messages with the required begin and
  end blocks to declare a string table.'''

  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    assert isinstance(lang, types.StringTypes)
    if begin_item:
      return 'STRINGTABLE\nBEGIN\n'
    else:
      return 'END\n\n'


class Message(interface.ItemFormatter):
  '''Writes out a single message to a string table.'''
  
  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    from grit.node import message
    if not begin_item:
      return ''
    
    assert isinstance(lang, types.StringTypes)
    assert isinstance(item, message.MessageNode)
    
    message = item.ws_at_start + item.Translate(lang) + item.ws_at_end
    # Escape quotation marks (RC format uses doubling-up
    message = message.replace('"', '""')
    # Replace linebreaks with a \n escape
    message = _LINEBREAKS.sub(r'\\n', message)
    
    name_attr = item.GetTextualIds()[0]
    
    return '  %-15s "%s"\n' % (name_attr, message)


class RcSection(interface.ItemFormatter):
  '''Writes out an .rc file section.'''
  
  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    if not begin_item:
      return ''
    
    assert isinstance(lang, types.StringTypes)
    from grit.node import structure
    assert isinstance(item, structure.StructureNode)
    
    if item.IsExcludedFromRc():
      return ''
    else:
      text = item.gatherer.Translate(
        lang, skeleton_gatherer=item.GetSkeletonGatherer(),
        pseudo_if_not_available=item.PseudoIsAllowed(),
        fallback_to_english=item.ShouldFallbackToEnglish()) + '\n\n'

      # Replace the language expand_variables in version rc info.
      unified_lang_code = GetUnifiedLangCode(lang)
      text = text.replace('[GRITVERLANGCHARSETHEX]', 
                          GetLangCharsetPair(unified_lang_code))
      text = text.replace('[GRITVERLANGID]', GetLangIdHex(unified_lang_code))
      text = text.replace('[GRITVERCHARSETID]',
                          GetCharsetIdDecimal(unified_lang_code))

      return text


class RcInclude(interface.ItemFormatter):
  '''Writes out an item that is included in an .rc file (e.g. an ICON)'''
  
  def __init__(self, type, filenameWithoutPath = 0, relative_path = 0):
    '''Indicates to the instance what the type of the resource include is,
    e.g. 'ICON' or 'HTML'.  Case must be correct, i.e. if the type is all-caps
    the parameter should be all-caps.
    
    Args:
      type: 'ICON'
    '''
    self.type_ = type
    self.filenameWithoutPath = filenameWithoutPath
    self.relative_path_ = relative_path
  
  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    if not begin_item:
      return ''

    assert isinstance(lang, types.StringTypes)
    from grit.node import structure
    from grit.node import include
    assert isinstance(item, (structure.StructureNode, include.IncludeNode))
    assert (isinstance(item, include.IncludeNode) or
            item.attrs['type'] in ['tr_html', 'admin_template', 'txt', 'muppet'])
        
    # By default, we use relative pathnames to included resources so that
    # sharing the resulting .rc files is possible.
    #
    # The FileForLanguage() Function has the side effect of generating the file
    # if needed (e.g. if it is an HTML file include).
    filename = os.path.abspath(item.FileForLanguage(lang, output_dir))
    if self.filenameWithoutPath:
      filename = os.path.basename(filename)
    elif self.relative_path_:
      filename = _MakeRelativePath(output_dir, filename)
    filename = filename.replace('\\', '\\\\')  # escape for the RC format

    if isinstance(item, structure.StructureNode) and item.IsExcludedFromRc():
      return ''
    else:
      return '%-18s %-18s "%s"\n' % (item.attrs['name'], self.type_, filename)

