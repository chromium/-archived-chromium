#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A gatherer for the TotalRecall brand of HTML templates with replaceable
portions.  We wanted to reuse extern.tclib.api.handlers.html.TCHTMLParser
but this proved impossible due to the fact that the TotalRecall HTML templates
are in general quite far from parseable HTML and the TCHTMLParser derives
from HTMLParser.HTMLParser which requires relatively well-formed HTML.  Some
examples of "HTML" from the TotalRecall HTML templates that wouldn't be
parseable include things like:

  <a [PARAMS]>blabla</a>  (not parseable because attributes are invalid)

  <table><tr><td>[LOTSOFSTUFF]</tr></table> (not parseable because closing
                                            </td> is in the HTML [LOTSOFSTUFF]
                                            is replaced by)

The other problem with using general parsers (such as TCHTMLParser) is that
we want to make sure we output the TotalRecall template with as little changes
as possible in terms of whitespace characters, layout etc.  With any parser
that generates a parse tree, and generates output by dumping the parse tree,
we would always have little inconsistencies which could cause bugs (the
TotalRecall template stuff is quite brittle and can break if e.g. a tab
character is replaced with spaces).

The solution, which may be applicable to some other HTML-like template
languages floating around Google, is to create a parser with a simple state
machine that keeps track of what kind of tag it's inside, and whether it's in
a translateable section or not.  Translateable sections are:

a) text (including [BINGO] replaceables) inside of tags that
   can contain translateable text (which is all tags except
   for a few)

b) text inside of an 'alt' attribute in an <image> element, or
   the 'value' attribute of a <submit>, <button> or <text>
   element.

The parser does not build up a parse tree but rather a "skeleton" which
is a list of nontranslateable strings intermingled with grit.clique.MessageClique
objects.  This simplifies the parser considerably compared to a regular HTML
parser.  To output a translated document, each item in the skeleton is
printed out, with the relevant Translation from each MessageCliques being used
for the requested language.

This implementation borrows some code, constants and ideas from
extern.tclib.api.handlers.html.TCHTMLParser.
'''


import re
import types

from grit import clique
from grit import exception
from grit import util
from grit import tclib

from grit.gather import interface


# HTML tags which break (separate) chunks.
_BLOCK_TAGS = ['script', 'p', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'hr', 'br',
              'body', 'style', 'head', 'title', 'table', 'tr', 'td', 'th',
              'ul', 'ol', 'dl', 'nl', 'li', 'div', 'object', 'center',
              'html', 'link', 'form', 'select', 'textarea',
              'button', 'option', 'map', 'area', 'blockquote', 'pre',
              'meta', 'xmp', 'noscript', 'label', 'tbody', 'thead',
              'script', 'style', 'pre', 'iframe', 'img', 'input', 'nowrap']

# HTML tags which may appear within a chunk.
_INLINE_TAGS = ['b', 'i', 'u', 'tt', 'code', 'font', 'a', 'span', 'small',
               'key', 'nobr', 'url', 'em', 's', 'sup', 'strike',
               'strong']

# HTML tags within which linebreaks are significant.
_PREFORMATTED_TAGS = ['textarea', 'xmp', 'pre']

# An array mapping some of the inline HTML tags to more meaningful
# names for those tags.  This will be used when generating placeholders
# representing these tags.
_HTML_PLACEHOLDER_NAMES = { 'a' : 'link', 'br' : 'break', 'b' : 'bold',
  'i' : 'italic', 'li' : 'item', 'ol' : 'ordered_list', 'p' : 'paragraph',
  'ul' : 'unordered_list', 'img' : 'image', 'em' : 'emphasis' }

# We append each of these characters in sequence to distinguish between
# different placeholders with basically the same name (e.g. BOLD1, BOLD2).
# Keep in mind that a placeholder name must not be a substring of any other
# placeholder name in the same message, so we can't simply count (BOLD_1
# would be a substring of BOLD_10).
_SUFFIXES = '123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'

# Matches whitespace in an HTML document.  Also matches HTML comments, which are
# treated as whitespace.
_WHITESPACE = re.compile(r'(\s|&nbsp;|\\n|\\r|<!--\s*desc\s*=.*?-->)+',
                         re.DOTALL)

# Finds a non-whitespace character
_NON_WHITESPACE = re.compile(r'\S')

# Matches two or more &nbsp; in a row (a single &nbsp is not changed into
# placeholders because different languages require different numbers of spaces
# and placeholders must match exactly; more than one is probably a "special"
# whitespace sequence and should be turned into a placeholder).
_NBSP = re.compile(r'&nbsp;(&nbsp;)+')

# Matches nontranslateable chunks of the document
_NONTRANSLATEABLES = re.compile(r'''
  <\s*script.+?<\s*/\s*script\s*>
  |
  <\s*style.+?<\s*/\s*style\s*>
  |
  <!--.+?-->
  |
  <\?IMPORT\s.+?>           # import tag
  |
  <\s*[a-zA-Z_]+:.+?>       # custom tag (open)
  |
  <\s*/\s*[a-zA-Z_]+:.+?>   # custom tag (close)
  |
  <!\s*[A-Z]+\s*([^>]+|"[^"]+"|'[^']+')*?>
  ''', re.MULTILINE | re.DOTALL | re.VERBOSE | re.IGNORECASE)

# Matches a tag and its attributes
_ELEMENT = re.compile(r'''
  # Optional closing /, element name
  <\s*(?P<closing>/)?\s*(?P<element>[a-zA-Z0-9]+)\s*
  # Attributes and/or replaceables inside the tag, if any
  (?P<atts>(
    \s*([a-zA-Z_][-:.a-zA-Z_0-9]*) # Attribute name
    (\s*=\s*(\'[^\']*\'|"[^"]*"|[-a-zA-Z0-9./,:;+*%?!&$\(\)_#=~\'"@]*))?
    |
    \s*\[(\$?\~)?([A-Z0-9-_]+?)(\~\$?)?\]
  )*)
  \s*(?P<empty>/)?\s*> # Optional empty-tag closing /, and tag close
  ''',
  re.MULTILINE | re.DOTALL | re.VERBOSE)

# Matches elements that may have translateable attributes.  The value of these
# special attributes is given by group 'value1' or 'value2'.  Note that this
# regexp demands that the attribute value be quoted; this is necessary because
# the non-tree-building nature of the parser means we don't know when we're
# writing out attributes, so we wouldn't know to escape spaces.
_SPECIAL_ELEMENT = re.compile(r'''
  <\s*(
    input[^>]+?value\s*=\s*(\'(?P<value3>[^\']*)\'|"(?P<value4>[^"]*)")
    [^>]+type\s*=\s*"?'?(button|reset|text|submit)'?"?
    |
    (
      table[^>]+?title\s*=
      |
      img[^>]+?alt\s*=
      |
      input[^>]+?type\s*=\s*"?'?(button|reset|text|submit)'?"?[^>]+?value\s*=
    )
    \s*(\'(?P<value1>[^\']*)\'|"(?P<value2>[^"]*)")
  )[^>]*?>
  ''', re.MULTILINE | re.DOTALL | re.VERBOSE | re.IGNORECASE)

# Matches stuff that is translateable if it occurs in the right context
# (between tags).  This includes all characters and character entities.
# Note that this also matches &nbsp; which needs to be handled as whitespace
# before this regexp is applied.
_CHARACTERS = re.compile(r'''
  (
    \w
    |
    [\!\@\#\$\%\^\*\(\)\-\=\_\+\[\]\{\}\\\|\;\:\'\"\,\.\/\?\`\~]
    |
    &(\#[0-9]+|\#x[0-9a-fA-F]+|[A-Za-z0-9]+);
  )+
  ''', re.MULTILINE | re.DOTALL | re.VERBOSE)

# Matches Total Recall's "replaceable" tags, which are just any text
# in capitals enclosed by delimiters like [] or [~~] or [$~~$] (e.g. [HELLO],
# [~HELLO~] and [$~HELLO~$]).
_REPLACEABLE = re.compile(r'\[(\$?\~)?(?P<name>[A-Z0-9-_]+?)(\~\$?)?\]',
                          re.MULTILINE)


# Matches the silly [!]-prefixed "header" that is used in some TotalRecall
# templates.
_SILLY_HEADER = re.compile(r'\[!\]\ntitle\t(?P<title>[^\n]+?)\n.+?\n\n',
                           re.MULTILINE | re.DOTALL)


# Matches a comment that provides a description for the message it occurs in.
_DESCRIPTION_COMMENT = re.compile(
  r'<!--\s*desc\s*=\s*(?P<description>.+?)\s*-->', re.DOTALL)


_DEBUG = 0
def _DebugPrint(text):
  if _DEBUG:
    print text.encode('utf-8')


class HtmlChunks(object):
  '''A parser that knows how to break an HTML-like document into a list of
  chunks, where each chunk is either translateable or non-translateable.
  The chunks are unmodified sections of the original document, so concatenating
  the text of all chunks would result in the original document.'''

  def InTranslateable(self):
    return self.last_translateable != -1

  def Rest(self):
    return self.text_[self.current:]

  def StartTranslateable(self):
    assert not self.InTranslateable()
    if self.current != 0:
      # Append a nontranslateable chunk
      chunk_text = self.text_[self.chunk_start : self.last_nontranslateable + 1]
      # Needed in the case where document starts with a translateable.
      if len(chunk_text) > 0:
        self.AddChunk(False, chunk_text)
    self.chunk_start = self.last_nontranslateable + 1
    self.last_translateable = self.current
    self.last_nontranslateable = -1

  def EndTranslateable(self):
    assert self.InTranslateable()
    # Append a translateable chunk
    self.AddChunk(True,
                  self.text_[self.chunk_start : self.last_translateable + 1])
    self.chunk_start = self.last_translateable + 1
    self.last_translateable = -1
    self.last_nontranslateable = self.current

  def AdvancePast(self, match):
    self.current += match.end()

  def AddChunk(self, translateable, text):
    '''Adds a chunk to self, removing linebreaks and duplicate whitespace
    if appropriate.
    '''
    if translateable and not self.last_element_ in _PREFORMATTED_TAGS:
      text = text.replace('\n', ' ')
      text = text.replace('\r', ' ')
      text = text.replace('   ', ' ')
      text = text.replace('  ', ' ')

    m = _DESCRIPTION_COMMENT.search(text)
    if m:
      self.last_description = m.group('description')
      # remove the description from the output text
      text = _DESCRIPTION_COMMENT.sub('', text)

    if translateable:
      description = self.last_description
      self.last_description = ''
    else:
      description = ''

    if text != '':
      self.chunks_.append((translateable, text, description))

  def Parse(self, text):
    '''Parses self.text_ into an intermediate format stored in self.chunks_
    which is translateable and nontranslateable chunks.  Also returns
    self.chunks_

    Return:
      [chunk1, chunk2, chunk3, ...]  (instances of class Chunk)
    '''
    #
    # Chunker state
    #

    self.text_ = text

    # A list of tuples (is_translateable, text) which represents the document
    # after chunking.
    self.chunks_ = []

    # Start index of the last chunk, whether translateable or not
    self.chunk_start = 0

    # Index of the last for-sure translateable character if we are parsing
    # a translateable chunk, -1 to indicate we are not in a translateable chunk.
    # This is needed so that we don't include trailing whitespace in the
    # translateable chunk (whitespace is neutral).
    self.last_translateable = -1

    # Index of the last for-sure nontranslateable character if we are parsing
    # a nontranslateable chunk, -1 if we are not in a nontranslateable chunk.
    # This is needed to make sure we can group e.g. "<b>Hello</b> there"
    # together instead of just "Hello</b> there" which would be much worse
    # for translation.
    self.last_nontranslateable = -1

    # Index of the character we're currently looking at.
    self.current = 0

    # The name of the last block element parsed.
    self.last_element_ = ''

    # The last explicit description we found.
    self.last_description = ''

    while self.current < len(self.text_):
      _DebugPrint('REST: %s' % self.text_[self.current:self.current+60])

      # First try to match whitespace
      m = _WHITESPACE.match(self.Rest())
      if m:
        # Whitespace is neutral, it just advances 'current' and does not switch
        # between translateable/nontranslateable.  If we are in a
        # nontranslateable section that extends to the current point, we extend
        # it to include the whitespace.  If we are in a translateable section,
        # we do not extend it until we find
        # more translateable parts, because we never want a translateable chunk
        # to end with whitespace.
        if (not self.InTranslateable() and
            self.last_nontranslateable == self.current - 1):
          self.last_nontranslateable = self.current + m.end() - 1
        self.AdvancePast(m)
        continue

      # Then we try to match nontranslateables
      m = _NONTRANSLATEABLES.match(self.Rest())
      if m:
        if self.InTranslateable():
          self.EndTranslateable()
        self.last_nontranslateable = self.current + m.end() - 1
        self.AdvancePast(m)
        continue

      # Now match all other HTML element tags (opening, closing, or empty, we
      # don't care).
      m = _ELEMENT.match(self.Rest())
      if m:
        element_name = m.group('element').lower()
        if element_name in _BLOCK_TAGS:
          self.last_element_ = element_name
          if self.InTranslateable():
            self.EndTranslateable()

          # Check for "special" elements, i.e. ones that have a translateable
          # attribute, and handle them correctly.  Note that all of the
          # "special" elements are block tags, so no need to check for this
          # if the tag is not a block tag.
          sm = _SPECIAL_ELEMENT.match(self.Rest())
          if sm:
            # Get the appropriate group name
            for group in sm.groupdict().keys():
              if sm.groupdict()[group]:
                break

            # First make a nontranslateable chunk up to and including the
            # quote before the translateable attribute value
            self.AddChunk(False, self.text_[
              self.chunk_start : self.current + sm.start(group)])
            # Then a translateable for the translateable bit
            self.AddChunk(True, self.Rest()[sm.start(group) : sm.end(group)])
            # Finally correct the data invariant for the parser
            self.chunk_start = self.current + sm.end(group)

          self.last_nontranslateable = self.current + m.end() - 1
        elif self.InTranslateable():
          # We're in a translateable and the tag is an inline tag, so we
          # need to include it in the translateable.
          self.last_translateable = self.current + m.end() - 1
        self.AdvancePast(m)
        continue

      # Anything else we find must be translateable, so we advance one character
      # at a time until one of the above matches.
      if not self.InTranslateable():
        self.StartTranslateable()
      else:
        self.last_translateable = self.current
      self.current += 1

    # Close the final chunk
    if self.InTranslateable():
      self.AddChunk(True, self.text_[self.chunk_start : ])
    else:
      self.AddChunk(False, self.text_[self.chunk_start : ])

    return self.chunks_


def HtmlToMessage(html, include_block_tags=False, description=''):
  '''Takes a bit of HTML, which must contain only "inline" HTML elements,
  and changes it into a tclib.Message.  This involves escaping any entities and
  replacing any HTML code with placeholders.

  If include_block_tags is true, no error will be given if block tags (e.g.
  <p> or <br>) are included in the HTML.

  Args:
    html: 'Hello <b>[USERNAME]</b>, how&nbsp;<i>are</i> you?'
    include_block_tags: False

  Return:
    tclib.Message('Hello START_BOLD1USERNAMEEND_BOLD, '
                  'howNBSPSTART_ITALICareEND_ITALIC you?',
                  [ Placeholder('START_BOLD', '<b>', ''),
                    Placeholder('USERNAME', '[USERNAME]', ''),
                    Placeholder('END_BOLD', '</b>', ''),
                    Placeholder('START_ITALIC', '<i>', ''),
                    Placeholder('END_ITALIC', '</i>', ''), ])
  '''
  # Approach is:
  # - first placeholderize, finding <elements>, [REPLACEABLES] and &nbsp;
  # - then escape all character entities in text in-between placeholders

  parts = []  # List of strings (for text chunks) and tuples (ID, original)
              # for placeholders

  count_names = {}  # Map of base names to number of times used
  end_names = {}  # Map of base names to stack of end tags (for correct nesting)

  def MakeNameClosure(base, type = ''):
    '''Returns a closure that can be called once all names have been allocated
    to return the final name of the placeholder.  This allows us to minimally
    number placeholders for non-overlap.

    Also ensures that END_XXX_Y placeholders have the same Y as the
    corresponding BEGIN_XXX_Y placeholder when we have nested tags of the same
    type.

    Args:
      base: 'phname'
      type: '' | 'begin' | 'end'

    Return:
      Closure()
    '''
    name = base
    if type != '':
      name = ('%s_%s' % (type, base)).upper()

    if name in count_names.keys():
      count_names[name] += 1
    else:
      count_names[name] = 1

    def MakeFinalName(name_ = name, index = count_names[name] - 1):
      if (type.lower() == 'end' and
          base in end_names.keys() and len(end_names[base])):
        return end_names[base].pop(-1)  # For correct nesting
      if count_names[name_] != 1:
        name_ = '%s_%s' % (name_, _SUFFIXES[index])
        # We need to use a stack to ensure that the end-tag suffixes match
        # the begin-tag suffixes.  Only needed when more than one tag of the
        # same type.
        if type == 'begin':
          end_name = ('END_%s_%s' % (base, _SUFFIXES[index])).upper()
          if base in end_names.keys():
            end_names[base].append(end_name)
          else:
            end_names[base] = [end_name]

      return name_

    return MakeFinalName

  current = 0

  while current < len(html):
    m = _NBSP.match(html[current:])
    if m:
      parts.append((MakeNameClosure('SPACE'), m.group()))
      current += m.end()
      continue

    m = _REPLACEABLE.match(html[current:])
    if m:
      # Replaceables allow - but placeholders don't, so replace - with _
      ph_name = MakeNameClosure('X_%s_X' % m.group('name').replace('-', '_'))
      parts.append((ph_name, m.group()))
      current += m.end()
      continue

    m = _SPECIAL_ELEMENT.match(html[current:])
    if m:
      if not include_block_tags:
        raise exception.BlockTagInTranslateableChunk(html)
      element_name = 'block'  # for simplification
      # Get the appropriate group name
      for group in m.groupdict().keys():
        if m.groupdict()[group]:
          break
      parts.append((MakeNameClosure(element_name, 'begin'),
                    html[current : current + m.start(group)]))
      parts.append(m.group(group))
      parts.append((MakeNameClosure(element_name, 'end'),
                    html[current + m.end(group) : current + m.end()]))
      current += m.end()
      continue

    m = _ELEMENT.match(html[current:])
    if m:
      element_name = m.group('element').lower()
      if not include_block_tags and not element_name in _INLINE_TAGS:
        raise exception.BlockTagInTranslateableChunk(html[current:])
      if element_name in _HTML_PLACEHOLDER_NAMES:  # use meaningful names
        element_name = _HTML_PLACEHOLDER_NAMES[element_name]

      # Make a name for the placeholder
      type = ''
      if not m.group('empty'):
        if m.group('closing'):
          type = 'end'
        else:
          type = 'begin'
      parts.append((MakeNameClosure(element_name, type), m.group()))
      current += m.end()
      continue

    if len(parts) and isinstance(parts[-1], types.StringTypes):
      parts[-1] += html[current]
    else:
      parts.append(html[current])
    current += 1

  msg_text = ''
  placeholders = []
  for part in parts:
    if isinstance(part, types.TupleType):
      final_name = part[0]()
      original = part[1]
      msg_text += final_name
      placeholders.append(tclib.Placeholder(final_name, original, '(HTML code)'))
    else:
      msg_text += part

  msg = tclib.Message(text=msg_text, placeholders=placeholders,
                      description=description)
  content = msg.GetContent()
  for ix in range(len(content)):
    if isinstance(content[ix], types.StringTypes):
      content[ix] = util.UnescapeHtml(content[ix], replace_nbsp=False)

  return msg


class TrHtml(interface.GathererBase):
  '''Represents a document or message in the template format used by
  Total Recall for HTML documents.'''

  def __init__(self, text):
    '''Creates a new object that represents 'text'.
    Args:
      text: '<html>...</html>'
    '''
    super(type(self), self).__init__()

    self.text_ = text
    self.have_parsed_ = False
    self.skeleton_ = []  # list of strings and MessageClique objects

  def GetText(self):
    '''Returns the original text of the HTML document'''
    return self.text_

  def GetCliques(self):
    '''Returns the message cliques for each translateable message in the
    document.'''
    return filter(lambda x: isinstance(x, clique.MessageClique), self.skeleton_)

  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    '''Returns this document with translateable messages filled with
    the translation for language 'lang'.

    Args:
      lang: 'en'
      pseudo_if_not_available: True

    Return:
      'ID_THIS_SECTION TYPE\n...BEGIN\n  "Translated message"\n......\nEND

    Raises:
      grit.exception.NotReady() if used before Parse() has been successfully
      called.
      grit.exception.NoSuchTranslation() if 'pseudo_if_not_available' is false
      and there is no translation for the requested language.
    '''
    if len(self.skeleton_) == 0:
      raise exception.NotReady()

    # TODO(joi) Implement support for skeleton gatherers here.

    out = []
    for item in self.skeleton_:
      if isinstance(item, types.StringTypes):
        out.append(item)
      else:
        msg = item.MessageForLanguage(lang,
                                      pseudo_if_not_available,
                                      fallback_to_english)
        for content in msg.GetContent():
          if isinstance(content, tclib.Placeholder):
            out.append(content.GetOriginal())
          else:
            # We escape " characters to increase the chance that attributes
            # will be properly escaped.
            out.append(util.EscapeHtml(content, True))

    return ''.join(out)


  # Parsing is done in two phases:  First, we break the document into
  # translateable and nontranslateable chunks.  Second, we run through each
  # translateable chunk and insert placeholders for any HTML elements, unescape
  # escaped characters, etc.
  def Parse(self):
    if self.have_parsed_:
      return
    self.have_parsed_ = True

    text = self.text_

    # First handle the silly little [!]-prefixed header because it's not
    # handled by our HTML parsers.
    m = _SILLY_HEADER.match(text)
    if m:
      self.skeleton_.append(text[:m.start('title')])
      self.skeleton_.append(self.uberclique.MakeClique(
        tclib.Message(text=text[m.start('title'):m.end('title')])))
      self.skeleton_.append(text[m.end('title') : m.end()])
      text = text[m.end():]

    chunks = HtmlChunks().Parse(text)

    for chunk in chunks:
      if chunk[0]:  # Chunk is translateable
        self.skeleton_.append(self.uberclique.MakeClique(
          HtmlToMessage(chunk[1], description=chunk[2])))
      else:
        self.skeleton_.append(chunk[1])

    # Go through the skeleton and change any messages that consist solely of
    # placeholders and whitespace into nontranslateable strings.
    for ix in range(len(self.skeleton_)):
      got_text = False
      if isinstance(self.skeleton_[ix], clique.MessageClique):
        msg = self.skeleton_[ix].GetMessage()
        for item in msg.GetContent():
          if (isinstance(item, types.StringTypes) and _NON_WHITESPACE.search(item)
              and item != '&nbsp;'):
            got_text = True
            break
        if not got_text:
          self.skeleton_[ix] = msg.GetRealContent()


  # Static method
  def FromFile(html, extkey=None, encoding = 'utf-8'):
    '''Creates a TrHtml object from the contents of 'html' which are decoded
    using 'encoding'.  Returns a new TrHtml object, upon which Parse() has not
    been called.

    Args:
      html: file('') | 'filename.html'
      extkey: ignored
      encoding: 'utf-8' (note that encoding is ignored if 'html' is not a file
                         name but instead an open file or file-like object)

    Return:
      TrHtml(text_of_file)
    '''
    if isinstance(html, types.StringTypes):
      html = util.WrapInputStream(file(html, 'r'), encoding)
    doc = html.read()

    # Ignore the BOM character if the document starts with one.
    if len(doc) and doc[0] == u'\ufeff':
      doc = doc[1:]

    return TrHtml(doc)
  FromFile = staticmethod(FromFile)

