#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# purify_message.py

''' Utility objects and functions to parse and unique Purify messages '''

import cStringIO
import logging
import re
import sys

import google.logging_utils

# used to represent one or more elided frames
ELIDE = "..."
# used to represent stack truncation at a known entry point
TRUNCATE = "^^^"
# a file that's outside of our source directory
EXTERNAL_FILE = "EXTERNAL_FILE"

# mapping of purify message types to descriptions
message_type = {
  "ABR": "Array Bounds Read",
  "ABW": "Array Bounds Write",
  "ABWL": "Array Bounds Write (late detect)",
  "BSR": "Beyond Stack Read",
  "BSW": "Beyond Stack Write",
  "COM": "COM API/Interface Failure",
  "EXC": "Continued Exception",
  "EXH": "Handled Exception",
  "EXI": "Ignored Exception",
  "EXU": "Unhandled Exception",
  "FFM": "Freeing Freed Memory",
  "FIM": "Freeing Invalid Memory",
  "FMM": "Freeing Mismatched Memory",
  "FMR": "Free Memory Read",
  "FMW": "Free Memory Write",
  "FMWL": "Free Memory Write (late detect)",
  "HAN": "Invalid Handle",
  "HIU": "Handle In Use",
  "ILK": "COM Interface Leak",
  "IPR": "Invalid Pointer Read",
  "IPW": "Invalid Pointer Write",
  "MAF": "Memory Allocation Failure",
  "MIU": "Memory In Use",
  "MLK": "Memory Leak",
  "MPK": "Potential Memory Leak",
  "NPR": "Null Pointer Read",
  "NPW": "Null Pointer Write",
  "PAR": "Bad Parameter",
  "UMC": "Uninitialized Memory Copy",
  "UMR": "Uninitialized Memory Read",
}

# a magic message type which is not enumerated with the normal message type dict
FATAL = "FATAL"

def GetMessageType(key):
  if key in message_type:
    return message_type[key]
  elif key == FATAL:
    return key
  logging.warn("unknown message type %s" % key)
  return "UNKNOWN"

# currently unused, but here for documentation purposes
message_severity = {
  "I": "Informational",
  "E": "Error",
  "W": "Warning",
  "O": "Internal Purify Error",
}


class Stack:
  ''' A normalized Purify Stack.  The stack is constructed by adding one line
  at a time from a stack in a Purify text file via AddLine.
  Supports cmp and hash so that stacks which normalize the same can be sorted
  and uniqued.
  The original stack contents are preserved so that it's possible to drill
  down into the full details if necessary. '''

  # The top of the source tree.  This is stripped from the filename as part
  # of normalization.
  source_dir = ""

  @classmethod
  def SetSourceDir(cls, dir):
    # normalize the dir
    cls.source_dir = dir.replace("\\", "/").lower()
    logging.debug("Stack.source_dir = %s" % cls.source_dir)

  # a line in a stack trace
  pat_stack_line = re.compile('(.*)\[(\w:)?([^\:\s]*)(:\d+)?(\s+.*)?]')

  # Known stack entry points that allow us to truncate the rest of the stack
  # below that point.
  pat_known_entries = (
     re.compile('RunnableMethod::Run\(void\)'),
     re.compile('ChromeMain'),
     re.compile('BrowserMain'),
     re.compile('wWinMain'),
     re.compile('TimerManager::ProcessPendingTimer\(void\)'),
     re.compile('RunnableMethod::RunableMethod\(.*\)'),
     re.compile('RenderViewHost::OnMessageReceived\(Message::IPC const&\)'),
     re.compile('testing::Test::Run\(void\)'),
     re.compile('testing::TestInfoImpl::Run\(void\)'),
     re.compile('Thread::ThreadFunc\\(void \*\)'),
     re.compile('TimerTask::Run\(void\)'),
     re.compile('MessageLoop::RunTask\(Task \*\)'),
     re.compile('.DispatchToMethod\@.*'),
     )

  # if functions match the following, elide them from the stack
  pat_func_elide = (re.compile('^std::'), re.compile('^new\('))
  # if files match the following, elide them from the stack
  pat_file_elide = (re.compile('.*platformsdk_vista.*'), 
                    re.compile('.*.(dll|DLL)$'),
                    # bug 1069902
                    re.compile('webkit/pending/wtf/fastmalloc\.h'),
                    # When we leak sqlite stuff, we leak a lot, and the stacks
                    # are all over the place.  For now, let's assume that
                    # sqlite itself is leak free and focus on our calling code.
                    re.compile('third_party/sqlite/.*'),
                    )

  pat_unit_test = re.compile('^([a-zA-Z0-9]+)_(\w+)_Test::.*')

  def __init__(self, title):
    self._title = title.lstrip()
    self._stack = []
    self._orig = ""
    # are we currently in an eliding block
    self._eliding = False
    # have we truncated the stack?
    self._truncated = False
    # is the stack made up completely of external code? (i.e. elided)
    self._all_external = True
    # a logical group that this stack belongs to
    self._group = None
    # top stack line (preserved even if elided)
    self._top_stack_line = None

  def GetLines(self):
    return self._stack

  def GetTopStackLine(self):
    return self._top_stack_line

  def GetTopVisibleStackLine(self):
    for line in self._stack:
      if line['function']:
        return line
    return {}

  def GetGroup(self):
    '''A logical grouping for this stack, allowing related stacks to be grouped
    together.  Subgroups within a group are separated by ".".
    (e.g. group.subgroup.subsubgroup)
    '''
    return self._group;
    
  def _ComputeStackLine(self, line):
    line = line.lstrip()
    m = Stack.pat_stack_line.match(line)
    if m:
      func = m.group(1).rstrip()
      func = self._DemangleSymbol(func)
      func = self._DetemplatizeSymbol(func)
      if m.group(2):
        file = m.group(2) + m.group(3)
      else:
        file = m.group(3)
      # paths are normalized to use / and be lower case
      file = file.replace("\\", "/").lower()
      if not file.startswith(Stack.source_dir):
        file = EXTERNAL_FILE
      else:
        file = file[len(Stack.source_dir):]
        # trim leading / if present
        if file[0] == "/":
          file = file[1:]
      loc = m.group(4)
      if loc:
        loc = int(loc[1:])
      else:
        loc = 0
      return {'function': func, 'file': file, 'line_number': loc}
    return None

  def _ShouldElide(self, stack_line):
    func = stack_line['function']
    file = stack_line['file']
    # elide certain common functions from the stack such as the STL
    for pat in Stack.pat_func_elide:
      if pat.match(func):
        logging.debug("eliding due to func pat match: %s" % func)
        return True
    if file == EXTERNAL_FILE:
      # if it's not in our source tree, then elide
      logging.debug("eliding due to external file: %s" % file)
      return True
    # elide certain common file sources from the stack, usually this
    # involves system libraries
    for pat in Stack.pat_file_elide:
      if pat.match(file):
        logging.debug("eliding due to file pat match: %s" % file)
        return True

    return False

  def AddLine(self, line):
    ''' Add one line from a stack in a Purify text file.  Lines must be
    added in order (top down).  Lines are added to two internal structures:
    an original string copy and an array of normalized lines, split into
    (function, file, line number).
    Stack normalization does several things:
      * elides sections of the stack that are in external code
      * truncates the stack at so called "known entry points"
      * removes template type information from symbols
    Returns False if the line was elided or otherwise omitted.
    '''
    self._orig += line + "\n"
    stack_line = self._ComputeStackLine(line)
    if stack_line:
      if not self._top_stack_line:
        self._top_stack_line = stack_line
      # Unit test entry points are good groupings.  Even if we already have a
      # group set, a later unit-test stack line will override.
      # Note that we also do this even if the stack has already been truncated
      # since this is useful information.
      # TODO(erikkay): Maybe in this case, the truncation should be overridden?
      test_match = Stack.pat_unit_test.match(stack_line["function"])
      if test_match:
        self._group = test_match.group(1) + "." + test_match.group(2)

      if self._truncated:
        return False

      if self._ShouldElide(stack_line):
        if not self._eliding:
          self._eliding = True
          self._stack.append({'function': "", 'file': ELIDE, 'line_number': 0})
        return False
      else:
        self._stack.append(stack_line)
        self._eliding = False
        self._all_external = False
        
        # when we reach one of the known common stack entry points, truncate
        # the stack to avoid printing overly redundant information
        if len(self._stack) > 1:
          for f in Stack.pat_known_entries:
            if f.match(stack_line["function"]):
              if not self._group:
                # we're at the end of the stack, so use the path to the file
                # as the group if we don't already have one
                # This won't be incredibly reliable, but might still be useful.
                prev = self._stack[-2]
                if prev['file']:
                  self._group = '.'.join(prev['file'].split('/')[:-1])
              self._stack.append({'function': "", 'file': TRUNCATE,
                                 'line_number': 0})
              self._truncated = True
              return False
      return True
    else:
      # skip these lines
      logging.debug(">>>" + line)
      return False

  def _DemangleSymbol(self, symbol):
    # TODO(erikkay) - I'm not sure why Purify prepends an address on the
    # front of some of these as if it were a namespace (?A<addr>::).  From an
    # analysis standpoint, it seems meaningless and can change from machine to
    # machine, so it's best if it's thrown away
    if symbol.startswith("?A0x"):
      skipto = symbol.find("::")
      if skipto >= 0:
        symbol = symbol[(skipto+2):]
      else:
        logging.warn("unable to strip address off of symbol (%s)" % symbol)
    # TODO(erikkay) there are more symbols not being properly demangled
    # in Purify's output.  Some of these look like template-related issues.
    return symbol

  def _DetemplatizeSymbol(self, symbol):
    ''' remove all of the template arguments and return values from the
    symbol, normalizing it, making it more readable, and less precise '''
    ret = ""
    nested = 0
    for i in range(len(symbol)):
      if nested > 0:
        if symbol[i] == '>':
          nested -= 1
        elif symbol[i] == '<':
          nested += 1
      elif symbol[i] == '<':
        nested += 1
      else:
        ret += symbol[i]
    return ret

  def __hash__(self):
    return hash(self.NormalizedStr())

  def __cmp__(self, other):
    if not other:
      return 1
    len_self = len(self._stack)
    len_other = len(other._stack)
    min_len = min(len_self, len_other)
    # sort stacks from the bottom up    
    for i in range(-1, -(min_len + 1), -1):
      # compare file, then func, but omit line number
      ret = cmp((self._stack[i]['file'], self._stack[i]['function']),
                (other._stack[i]['file'], other._stack[i]['function']))
      if ret:
        return ret
    return cmp(len_self, len_other)

  def NormalizedStr(self, verbose=False):
    ''' String version of the normalized stack.  See AddLine for normalization
    details. '''
    # use cStringIO for more efficient string building
    out = cStringIO.StringIO()
    for line in self._stack:
      out.write("   ")
      out.write(line['file'])
      if verbose and line['line_number'] > 0:
        out.write(":%d" % line['line_number'])
      out.write("  ")
      out.write(line['function'])
      out.write("\n")
    ret = out.getvalue()
    out.close()
    return ret

  def __str__(self):
    return self._orig


class Message:
  '''A normalized message from a Purify text file.  Messages all have a
  severity, most have a type, and many have an error stack and/or an
  allocation stack.
  Supports cmp and hash so that messages which normalize the same can be
  sorted and uniqued.'''

  pat_count = re.compile('^(.*) \{(\d+) occurrences?\}')
  pat_leak = re.compile('(Potential )?[Mm]emory leak of (\d+) bytes? '
                        'from (\d+) blocks? allocated in (.+)')
  pat_miu = re.compile('Memory use of (\d+) bytes? '
                       '(\((\d+)% initialized\) )?from (\d+) blocks? '
                       'allocated .. (.+)')
  # these are headings to different types of stack traces
  pat_loc_error = re.compile('\s*(Exception|Error|Call) location')
  pat_loc_alloc = re.compile('\s*Allocation location')
  pat_loc_free = re.compile('\s*Free location')
  pat_loc_free2 = re.compile('\s*Location of free attempt')

  def __init__(self, severity, type, title):
    self._severity = severity
    self._type = type
    self._program = None
    self._head = ""
    self._loc_alloc = None
    self._loc_error = None
    self._loc_free = None
    self._stack = None
    self._count = 1
    self._bytes = 0
    self._blocks = 0
    m = Message.pat_count.match(title)
    if m:
      self._title = m.group(1)
      self._count = int(m.group(2))
    else:
      m = Message.pat_leak.match(title)
      if m:
        self._title = m.group(4)
        self._bytes = int(m.group(2))
        self._blocks = int(m.group(3))
      else:
        m = Message.pat_miu.match(title)
        if m:
          self._title = m.group(5)
          self._bytes = int(m.group(1))
          self._blocks = int(m.group(4))
          #print "%d/%d - %s" % (self._bytes, self._blocks, title[0:60])
        elif type == "MIU":
          logging.error("%s didn't match" % title)
          sys.exit(-1)
        else:
          self._title = title

  def GetAllocStack(self):
    return self._loc_alloc

  def GetErrorStack(self):
    return self._loc_error

  def GetGroup(self):
    '''An attempted logical grouping for this Message computed by the contained
    Stack objects.
    '''
    group = None
    if self._loc_alloc:
      group = self._loc_alloc.GetGroup()
    if not group and self._loc_error:
      group = self._loc_error.GetGroup()
    if not group and self._loc_free:
      group = self._loc_free.GetGroup()
    if not group:
      group = "UNKNOWN"
    return group

  def AddLine(self, line):
    '''Add a line one at a time (in order from the Purify text file) to
    build up the message and its associated stacks. '''

    if Message.pat_loc_error.match(line):
      self._stack = Stack(line)
      self._loc_error = self._stack
    elif Message.pat_loc_alloc.match(line):
      self._stack = Stack(line)
      self._loc_alloc = self._stack
    elif Message.pat_loc_free.match(line) or Message.pat_loc_free2.match(line):
      self._stack = Stack(line)
      self._loc_free = self._stack
    elif self._stack:
      if not line.startswith("            "):
        logging.debug("*** " + line)
      self._stack.AddLine(line)
    else:
      self._head += line.lstrip()

  def Type(self):
    return self._type

  def Program(self):
    return self._program

  def SetProgram(self, program):
    self._program = program

  def StacksAllExternal(self):
    '''Returns True if the stacks it contains are made up completely of
    external (elided) symbols'''
    return ((not self._loc_error or self._loc_error._all_external) and
            (not self._loc_alloc or self._loc_alloc._all_external) and
            (not self._loc_free or self._loc_free._all_external))

  def __hash__(self):
    # NOTE: see also _MessageHashesFromFile.  If this method changes, then
    # _MessageHashesFromFile must be updated to match.
    s = ""
    if self._loc_error:
      s += "Error Location\n" + self._loc_error.NormalizedStr()
    if self._loc_alloc:
      s += "Alloc Location\n" + self._loc_alloc.NormalizedStr()
    if self._loc_free:
      s += "Free Location\n" + self._loc_free.NormalizedStr()
    return hash(s)

  def NormalizedStr(self, verbose=False):
    '''String version of the normalized message. Only includes title
    and normalized versions of error and allocation stacks if present.
    Example:
    Unitialized Memory Read in Foo::Bar()
    Error Location
      foo/Foo.cc  Foo::Bar(void)
      foo/main.cc start(void)
      foo/main.cc main(void)
    Alloc Location
      foo/Foo.cc  Foo::Foo(void)
      foo/main.cc start(void)
      foo/main.cc main(void)
    '''
    ret = ""
    # some of the message types are more verbose than others and we
    # don't need to indicate their type
    if verbose and self._type not in ["UMR", "IPR", "IPW"]:
      ret += GetMessageType(self._type) + ": "
    if verbose and self._bytes > 0:
      ret += "(%d bytes, %d blocks) " % (self._bytes, self._blocks)
    ret += "%s\n" % self._title
    if self._loc_error:
      ret += "Error Location\n" + self._loc_error.NormalizedStr(verbose)
    if self._loc_alloc:
      ret += "Alloc Location\n" + self._loc_alloc.NormalizedStr(verbose)
    if self._loc_free:
      ret += "Free Location\n" + self._loc_free.NormalizedStr(verbose)
    return ret

  def __str__(self):
    ret = self._title + "\n" + self._head
    if self._loc_error:
      ret += "Error Location\n" + str(self._loc_error)
    if self._loc_alloc:
      ret += "Alloc Location\n" + str(self._loc_alloc)
    if self._loc_free:
      ret += "Free Location\n" + str(self._loc_free)
    return ret

  def __cmp__(self, other):
    if not other:
      return 1
    ret = 0
    if self._loc_error:
      ret = cmp(self._loc_error, other._loc_error)
    if ret == 0 and self._loc_alloc:
      ret = cmp(self._loc_alloc, other._loc_alloc)
    if ret == 0 and self._loc_free:
      ret = cmp(self._loc_free, other._loc_free)
    # since title is often not very interesting, we sort against that last
    if ret == 0:
      ret = cmp(self._title, other._title)
    return ret


class MessageList:
  '''A collection of Message objects of a given message type.'''
  def __init__(self, type):
    self._type = type
    self._messages = []
    self._unique_messages = None
    self._sublists = None
    self._bytes = 0
    
  def GetType(self):
    return self._type

  def BeginNewSublist(self):  
    '''Some message types are logically grouped into sets of messages which
    should not be mixed in the same list.  Specifically, Memory In Use (MIU),
    Memory Leak (MLK) and Potential Memory Leak (MPK) are generated in a set
    all at once, but this generation can happen at multiple distinct times,
    either via the Purify UI or through Purify API calls.  For example, if
    Purify is told to dump a list all memory leaks once, and then a few minutes
    later, the two lists will certainly overlap, so they should be kept
    in separate lists.
    In order to accommodate this, MessageList supports the notion of sublists.
    When the caller determines that one list of messages of a type has ended
    and a new list has begun, it calls BeginNewSublist() which takes the current
    set of messages, puts them into a new MessageList and puts that into the
    sublists array.  Later, when the caller needs to get at these messages, 
    GetSublists() should be called.
    '''
    if len(self._messages):
      # if this is the first list, no need to make a new one
      list = MessageList(self._type)
      list._messages = self._messages
      if not self._sublists:
        self._sublists = [list]
      else:
        self._sublists.append(list)
      self._messages = []
      logging.info("total size: %d" % self._bytes)
      self._bytes = 0

  def GetSublists(self):
    '''Returns the current list of sublists.  If there are currently sublists
    and there are any messages that aren't in a sublist, BeginNewSublist() is
    called implicitly by this method to force those ungrouped messages into
    their own sublist.
    '''
    if self._sublists and len(self._sublists) and len(self._messages):
      self.BeginNewSublist()
    return self._sublists

  def AddMessage(self, msg):
    '''Adds a message to this MessageList.'''
    # TODO(erikkay): assert if _unique_messages exists
    self._messages.append(msg)
    self._bytes += msg._bytes

  def AllMessages(self):
    '''Returns an array of all Message objects in this MessageList. '''
    # TODO(erikkay): handle case with sublists
    return self._messages

  def UniqueMessages(self):
    '''Returns an array of the unique normalized Message objects in this 
    MessageList.
    '''
    # the list is lazily computed since we have to create a sorted list,
    # which is only valid once all messages have been added
    # TODO(erikkay): handle case with sublists
    if not self._unique_messages:
      self._unique_messages = list(set(self._messages))
      self._unique_messages.sort()
    return self._unique_messages

  def UniqueMessageGroups(self):
    '''Returns a dictionary mapping Message group names to arrays of uniqued
    normalized Message objects in this MessageList.
    '''
    unique = self.UniqueMessages()
    groups = {}
    for msg in unique:
      group = msg.GetGroup()
      if not group in groups:
        groups[group] = []
      groups[group].append(msg)
    return groups

