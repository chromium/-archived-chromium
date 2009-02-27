#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Base types for nodes in a GRIT resource tree.
'''

import os
import sys
import types
from xml.sax import saxutils

from grit import exception
from grit import util
from grit import clique
import grit.format.interface


class Node(grit.format.interface.ItemFormatter):
  '''An item in the tree that has children.  Also implements the
  ItemFormatter interface to allow formatting a node as a GRD document.'''

  # Valid content types that can be returned by _ContentType()
  _CONTENT_TYPE_NONE = 0   # No CDATA content but may have children
  _CONTENT_TYPE_CDATA = 1  # Only CDATA, no children.
  _CONTENT_TYPE_MIXED = 2  # CDATA and children, possibly intermingled
  
  def __init__(self):
    self.children = []        # A list of child elements
    self.mixed_content = []   # A list of u'' and/or child elements (this
                              # duplicates 'children' but
                              # is needed to preserve markup-type content).
    self.name = u''           # The name of this element
    self.attrs = {}           # The set of attributes (keys to values)
    self.parent = None        # Our parent unless we are the root element.
    self.uberclique = None    # Allows overriding uberclique for parts of tree
  
  def __iter__(self):
    '''An in-order iteration through the tree that this node is the
    root of.'''
    return self.inorder()
  
  def inorder(self):
    '''Generator that generates first this node, then the same generator for
    any child nodes.'''
    yield self
    for child in self.children:
      for iterchild in child.inorder():
        yield iterchild
  
  def GetRoot(self):
    '''Returns the root Node in the tree this Node belongs to.'''
    curr = self
    while curr.parent:
      curr = curr.parent
    return curr
  
    # TODO(joi) Use this (currently untested) optimization?:
    #if hasattr(self, '_root'):
    #  return self._root
    #curr = self
    #while curr.parent and not hasattr(curr, '_root'):
    #  curr = curr.parent
    #if curr.parent:
    #  self._root = curr._root
    #else:
    #  self._root = curr
    #return self._root
    
  def StartParsing(self, name, parent):
    '''Called at the start of parsing.
    
    Args:
      name: u'elementname'
      parent: grit.node.base.Node or subclass or None
    '''
    assert isinstance(name, types.StringTypes)
    assert not parent or isinstance(parent, Node)
    self.name = name
    self.parent = parent

  def AddChild(self, child):
    '''Adds a child to the list of children of this node, if it is a valid
    child for the node.'''
    assert isinstance(child, Node)
    if (not self._IsValidChild(child) or
        self._ContentType() == self._CONTENT_TYPE_CDATA):
      if child.parent:
        explanation = 'child %s of parent %s' % (child.name, child.parent.name)
      else:
        explanation = 'node %s with no parent' % child.name
      raise exception.UnexpectedChild(explanation)
    self.children.append(child)
    self.mixed_content.append(child)

  def RemoveChild(self, child_id):
    '''Removes the first node that has a "name" attribute which
    matches "child_id" in the list of immediate children of
    this node.

    Args:
      child_id: String identifying the child to be removed
    '''
    index = 0
    # Safe not to copy since we only remove the first element found
    for child in self.children:
      name_attr = child.attrs['name']
      if name_attr == child_id:
        self.children.pop(index)
        self.mixed_content.pop(index)
        break
      index += 1
      
  def AppendContent(self, content):
    '''Appends a chunk of text as content of this node.
    
    Args:
      content: u'hello'
    
    Return:
      None
    '''
    assert isinstance(content, types.StringTypes)
    if self._ContentType() != self._CONTENT_TYPE_NONE:
      self.mixed_content.append(content)
    elif content.strip() != '':
      raise exception.UnexpectedContent()
    
  def HandleAttribute(self, attrib, value):
    '''Informs the node of an attribute that was parsed out of the GRD file
    for it.
    
    Args:
      attrib: 'name'
      value: 'fooblat'
    
    Return:
      None
    '''
    assert isinstance(attrib, types.StringTypes)
    assert isinstance(value, types.StringTypes)
    if self._IsValidAttribute(attrib, value):
      self.attrs[attrib] = value
    else:
      raise exception.UnexpectedAttribute(attrib)
  
  def EndParsing(self):
    '''Called at the end of parsing.'''
    
    # TODO(joi) Rewrite this, it's extremely ugly!
    if len(self.mixed_content):
      if isinstance(self.mixed_content[0], types.StringTypes):
        # Remove leading and trailing chunks of pure whitespace.
        while (len(self.mixed_content) and
               isinstance(self.mixed_content[0], types.StringTypes) and
               self.mixed_content[0].strip() == ''):
          self.mixed_content = self.mixed_content[1:]
        # Strip leading and trailing whitespace from mixed content chunks
        # at front and back.
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[0], types.StringTypes)):
          self.mixed_content[0] = self.mixed_content[0].lstrip()
        # Remove leading and trailing ''' (used to demarcate whitespace)
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[0], types.StringTypes)):
          if self.mixed_content[0].startswith("'''"):
            self.mixed_content[0] = self.mixed_content[0][3:]
    if len(self.mixed_content):
      if isinstance(self.mixed_content[-1], types.StringTypes):
        # Same stuff all over again for the tail end.
        while (len(self.mixed_content) and
               isinstance(self.mixed_content[-1], types.StringTypes) and
               self.mixed_content[-1].strip() == ''):
          self.mixed_content = self.mixed_content[:-1]
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[-1], types.StringTypes)):
          self.mixed_content[-1] = self.mixed_content[-1].rstrip()
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[-1], types.StringTypes)):
          if self.mixed_content[-1].endswith("'''"):
            self.mixed_content[-1] = self.mixed_content[-1][:-3]
    
    # Check that all mandatory attributes are there.
    for node_mandatt in self.MandatoryAttributes():
      mandatt_list = [] 
      if node_mandatt.find('|') >= 0:
        mandatt_list = node_mandatt.split('|')
      else:
        mandatt_list.append(node_mandatt)
      
      mandatt_option_found = False
      for mandatt in mandatt_list:
        assert mandatt not in self.DefaultAttributes().keys()
        if mandatt in self.attrs:
          if not mandatt_option_found:
            mandatt_option_found = True
          else:
            raise exception.MutuallyExclusiveMandatoryAttribute(mandatt)
          
      if not mandatt_option_found:   
        raise exception.MissingMandatoryAttribute(mandatt)
    
    # Add default attributes if not specified in input file.
    for defattr in self.DefaultAttributes():
      if not defattr in self.attrs:
        self.attrs[defattr] = self.DefaultAttributes()[defattr]
  
  def GetCdata(self):
    '''Returns all CDATA of this element, concatenated into a single
    string.  Note that this ignores any elements embedded in CDATA.'''
    return ''.join(filter(lambda c: isinstance(c, types.StringTypes),
                          self.mixed_content))
  
  def __unicode__(self):
    '''Returns this node and all nodes below it as an XML document in a Unicode
    string.'''
    header = u'<?xml version="1.0" encoding="UTF-8"?>\n'
    return header + self.FormatXml()
  
  # Compliance with ItemFormatter interface.
  def Format(self, item, lang_re = None, begin_item=True):
    if not begin_item:
      return ''
    else:
      return item.FormatXml()
    
  def FormatXml(self, indent = u'', one_line = False):
    '''Returns this node and all nodes below it as an XML
    element in a Unicode string.  This differs from __unicode__ in that it does
    not include the <?xml> stuff at the top of the string.  If one_line is true,
    children and CDATA are layed out in a way that preserves internal
    whitespace.
    '''
    assert isinstance(indent, types.StringTypes)
    
    content_one_line = (one_line or
                        self._ContentType() == self._CONTENT_TYPE_MIXED)
    inside_content = self.ContentsAsXml(indent, content_one_line)
    
    # Then the attributes for this node.
    attribs = u' '
    for (attrib, value) in self.attrs.iteritems():
      # Only print an attribute if it is other than the default value.
      if (not self.DefaultAttributes().has_key(attrib) or
          value != self.DefaultAttributes()[attrib]):
        attribs += u'%s=%s ' % (attrib, saxutils.quoteattr(value))
    attribs = attribs.rstrip()  # if no attribs, we end up with '', otherwise
                                # we end up with a space-prefixed string
    
    # Finally build the XML for our node and return it
    if len(inside_content) > 0:
      if one_line:
        return u'<%s%s>%s</%s>' % (self.name, attribs, inside_content, self.name)
      elif content_one_line:
        return u'%s<%s%s>\n%s  %s\n%s</%s>' % (
          indent, self.name, attribs,
          indent, inside_content,
          indent, self.name)
      else:
        return u'%s<%s%s>\n%s\n%s</%s>' % (
          indent, self.name, attribs,
          inside_content,
          indent, self.name)
    else:
      return u'%s<%s%s />' % (indent, self.name, attribs)
  
  def ContentsAsXml(self, indent, one_line):
    '''Returns the contents of this node (CDATA and child elements) in XML
    format.  If 'one_line' is true, the content will be laid out on one line.'''
    assert isinstance(indent, types.StringTypes)
    
    # Build the contents of the element.
    inside_parts = []
    last_item = None
    for mixed_item in self.mixed_content:
      if isinstance(mixed_item, Node):
        inside_parts.append(mixed_item.FormatXml(indent + u'  ', one_line))
        if not one_line:
          inside_parts.append(u'\n')
      else:
        message = mixed_item
        # If this is the first item and it starts with whitespace, we add
        # the ''' delimiter.
        if not last_item and message.lstrip() != message:
          message = u"'''" + message
        inside_parts.append(util.EncodeCdata(message))
      last_item = mixed_item

    # If there are only child nodes and no cdata, there will be a spurious
    # trailing \n
    if len(inside_parts) and inside_parts[-1] == '\n':
      inside_parts = inside_parts[:-1]
    
    # If the last item is a string (not a node) and ends with whitespace,
    # we need to add the ''' delimiter.
    if (isinstance(last_item, types.StringTypes) and
        last_item.rstrip() != last_item):
      inside_parts[-1] = inside_parts[-1] + u"'''"

    return u''.join(inside_parts)
  
  def RunGatherers(self, recursive=0, debug=False):
    '''Runs all gatherers on this object, which may add to the data stored
    by the object.  If 'recursive' is true, will call RunGatherers() recursively
    on all child nodes first.  If 'debug' is True, will print out information
    as it is running each nodes' gatherers.
    
    Gatherers for <translations> child nodes will always be run after all other
    child nodes have been gathered.
    '''
    if recursive:
      process_last = []
      for child in self.children:
        if child.name == 'translations':
          process_last.append(child)
        else:
          child.RunGatherers(recursive=recursive, debug=debug)
      for child in process_last:
        child.RunGatherers(recursive=recursive, debug=debug)
  
  def ItemFormatter(self, type):
    '''Returns an instance of the item formatter for this object of the
    specified type, or None if not supported.
    
    Args:
      type: 'rc-header'
    
    Return:
      (object RcHeaderItemFormatter)
    '''
    if type == 'xml':
      return self
    else:
      return None
  
  def SatisfiesOutputCondition(self):
    '''Returns true if this node is either not a child of an <if> element
    or if it is a child of an <if> element and the conditions for it being
    output are satisfied.
    
    Used to determine whether to return item formatters for formats that
    obey conditional output of resources (e.g. the RC formatters).
    '''
    from grit.node import misc
    if not self.parent or not isinstance(self.parent, misc.IfNode):
      return True
    else:
      return self.parent.IsConditionSatisfied()

  def _IsValidChild(self, child):
    '''Returns true if 'child' is a valid child of this node.
    Overridden by subclasses.'''
    return False

  def _IsValidAttribute(self, name, value):
    '''Returns true if 'name' is the name of a valid attribute of this element
    and 'value' is a valid value for that attribute.  Overriden by
    subclasses unless they have only mandatory attributes.'''
    return (name in self.MandatoryAttributes() or
            name in self.DefaultAttributes())
  
  def _ContentType(self):
    '''Returns the type of content this element can have.  Overridden by
    subclasses.  The content type can be one of the _CONTENT_TYPE_XXX constants
    above.'''
    return self._CONTENT_TYPE_NONE

  def MandatoryAttributes(self):
    '''Returns a list of attribute names that are mandatory (non-optional)
    on the current element. One can specify a list of 
    "mutually exclusive mandatory" attributes by specifying them as one
    element in the list, separated by a "|" character.
    '''
    return []

  def DefaultAttributes(self):
    '''Returns a dictionary of attribute names that have defaults, mapped to
    the default value.  Overridden by subclasses.'''
    return {}
  
  def GetCliques(self):
    '''Returns all MessageClique objects belonging to this node.  Overridden
    by subclasses.
    
    Return:
      [clique1, clique2] or []
    '''
    return []

  def ToRealPath(self, path_from_basedir):
    '''Returns a real path (which can be absolute or relative to the current
    working directory), given a path that is relative to the base directory
    set for the GRIT input file.
    
    Args:
      path_from_basedir: '..'
    
    Return:
      'resource'
    '''
    return util.normpath(os.path.join(self.GetRoot().GetBaseDir(),
                                      path_from_basedir))

  def FilenameToOpen(self):
    '''Returns a path, either absolute or relative to the current working
    directory, that points to the file the node refers to.  This is only valid
    for nodes that have a 'file' or 'path' attribute.  Note that the attribute
    is a path to the file relative to the 'base-dir' of the .grd file, whereas
    this function returns a path that can be used to open the file.'''
    file_attribute = 'file'
    if not file_attribute in self.attrs:
      file_attribute = 'path'
    return self.ToRealPath(self.attrs[file_attribute])

  def UberClique(self):
    '''Returns the uberclique that should be used for messages originating in
    a given node.  If the node itself has its uberclique set, that is what we
    use, otherwise we search upwards until we find one.  If we do not find one
    even at the root node, we set the root node's uberclique to a new
    uberclique instance.
    '''
    node = self
    while not node.uberclique and node.parent:
      node = node.parent
    if not node.uberclique:
      node.uberclique = clique.UberClique()
    return node.uberclique
  
  def IsTranslateable(self):
    '''Returns false if the node has contents that should not be translated,
    otherwise returns false (even if the node has no contents).
    '''
    if not 'translateable' in self.attrs:
      return True
    else:
      return self.attrs['translateable'] == 'true'

  def GetNodeById(self, id):
    '''Returns the node in the subtree parented by this node that has a 'name'
    attribute matching 'id'.  Returns None if no such node is found.
    '''
    for node in self:
      if 'name' in node.attrs and node.attrs['name'] == id:
        return node
    return None

  def GetTextualIds(self):
    '''Returns the textual ids of this node, if it has some.
    Otherwise it just returns None.
    '''
    if 'name' in self.attrs:
      return [self.attrs['name']]
    return None  

  def EvaluateCondition(self, expr):
    '''Returns true if and only if the Python expression 'expr' evaluates
    to true.
    
    The expression is given a few local variables:
      - 'lang' is the language currently being output
      - 'defs' is a map of C preprocessor-style define names to their values
      - 'os' is the current platform (likely 'linux2', 'win32' or 'darwin').
      - 'pp_ifdef(define)' which behaves just like the C preprocessors #ifdef,
        i.e. it is shorthand for "define in defs"
      - 'pp_if(define)' which behaves just like the C preprocessor's #if, i.e.
        it is shorthand for "define in defs and defs[define]".
    '''
    root = self.GetRoot()
    lang = ''
    defs = {}
    def pp_ifdef(define):
      return define in defs
    def pp_if(define):
      return define in defs and defs[define]
    if hasattr(root, 'output_language'):
      lang = root.output_language
    if hasattr(root, 'defines'):
      defs = root.defines
    return eval(expr, {},
                {'lang' : lang,
                 'defs' : defs,
                 'os': sys.platform,
                 'pp_ifdef' : pp_ifdef,
                 'pp_if' : pp_if})
  
  def OnlyTheseTranslations(self, languages):
    '''Turns off loading of translations for languages not in the provided list.
    
    Attrs:
      languages: ['fr', 'zh_cn']
    '''
    for node in self:
      if (hasattr(node, 'IsTranslation') and
          node.IsTranslation() and
          node.GetLang() not in languages):
        node.DisableLoading()
  
  def PseudoIsAllowed(self):
    '''Returns true if this node is allowed to use pseudo-translations.  This
    is true by default, unless this node is within a <release> node that has
    the allow_pseudo attribute set to false.
    '''
    p = self.parent
    while p:
      if 'allow_pseudo' in p.attrs:
        return (p.attrs['allow_pseudo'].lower() == 'true')
      p = p.parent
    return True
  
  def ShouldFallbackToEnglish(self):
    '''Returns true iff this node should fall back to English when
    pseudotranslations are disabled and no translation is available for a
    given message.
    '''
    p = self.parent
    while p:
      if 'fallback_to_english' in p.attrs:
        return (p.attrs['fallback_to_english'].lower() == 'true')
      p = p.parent
    return False

class ContentNode(Node):
  '''Convenience baseclass for nodes that can have content.'''
  def _ContentType(self):
    return self._CONTENT_TYPE_MIXED

