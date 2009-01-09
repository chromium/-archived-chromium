#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# purify_analyze.py

''' Given a Purify text file, parses messages, normalizes and uniques them.
If there's an existing baseline of this data, it can compare against that
baseline and return an error code if there are any new errors not in the
baseline. '''

import logging
import optparse
import os
import re
import sys

import google.logging_utils
import google.path_utils

import purify_message

class MemoryTreeNode(object):
  ''' A node in a tree representing stack traces of memory allocation.
  Essentially, each node in the tree is a hashtable mapping a child
  function name to a child node.  Each node contains the total number
  of bytes of all of its descendants.
  See also: PurifyAnalyze.PrintMemoryInUse()
  '''

  pat_initializer = re.compile('(.*)\`dynamic initializer for \'(.*)\'\'')

  @classmethod
  def CreateTree(cls, message_list):
    '''Creates a tree from message_list. All of the Message objects are built
       into a tree with a default "ROOT" root node that is then returned.
    Args:
      message_list: a MessageList object.
    '''
    root = MemoryTreeNode("ROOT", 0, 0)
    msgs = message_list.AllMessages()
    for msg in msgs:
      bytes = msg._bytes
      blocks = msg._blocks
      stack = msg.GetAllocStack()
      stack_lines = stack.GetLines()
      size = len(stack_lines)
      node = root
      node._AddAlloc(bytes, blocks)
      counted = False
      # process stack lines from the bottom up to build a call-stack tree
      functions = [line["function"] for line in stack_lines]
      functions.reverse()
      for func in functions:
        if node == root:
          m = MemoryTreeNode.pat_initializer.match(func)
          if m:
            node = node._AddChild("INITIALIZERS", bytes, blocks)
            func = m.group(1) + m.group(2)
        # don't process ellided or truncated stack lines
        if func:
          node = node._AddChild(func, bytes, blocks)
          counted = True
      if not counted:
        # Nodes with no stack frames in our code wind up not being counted
        # above.  These seem to be attributable to Windows DLL
        # initialization, so just throw them into that bucket.
        node._AddChild("WINDOWS", bytes, blocks)
    return root

  def __init__(self, function, bytes, blocks):
    ''' 
    Args:
      function: A string representing a unique method or function.
      bytes: initial number of bytes allocated in this node
      blocks: initial number of blocks allocated in this node
    '''
    self._function = function
    self._bytes = bytes
    self._blocks = blocks
    self._allocs = 1
    self._children = {}

  def _AddAlloc(self, bytes, blocks):
    '''Adds bytes and blocks to this node's allocation totals
    '''
    self._allocs += 1
    self._bytes += bytes
    self._blocks += blocks

  def _AddChild(self, function, bytes, blocks):
    '''Adds a child node if not present.  Otherwise, adds
    bytes and blocks to it's allocation total.
    '''
    if function not in self._children:
      self._children[function] = MemoryTreeNode(function, bytes, blocks)
    else:
      self._children[function]._AddAlloc(bytes, blocks)
    return self._children[function]

  def __cmp__(self, other):
    # sort by size, then blocks, then function name
    return cmp((self._bytes, self._blocks, self._function),
               (other._bytes, other._blocks, other._function))

  def __str__(self):
    return "(%d bytes, %d blocks, %d allocs) %s" % ( 
        self._bytes, self._blocks, self._allocs, self._function)

  def PrintRecursive(self, padding="", byte_filter=0):
    '''Print the tree and all of its children recursively (depth-first).  All
    nodes at a given level of the tree are sorted in descending order by size.
    
    Args:
      padding: Printed at the front of the line.  Each recursive call adds a 
        single space character.
      byte_filter: a number of bytes below which we'll prune the tree
    '''
    print "%s%s" % (padding, self)
    padding = padding + " "
    # sort the children in descending order (see __cmp__)
    swapped = self._children.values()
    swapped.sort(reverse=True)
    rest_bytes = 0
    rest_blocks = 0
    rest_allocs = 0
    for node in swapped:
      if node._bytes < byte_filter:
        rest_bytes += node._bytes
        rest_blocks += node._blocks
        rest_allocs += node._allocs
      else:
        node.PrintRecursive(padding, byte_filter)
    if rest_bytes:
      print "%s(%d bytes, %d blocks, %d allocs) PRUNED" % (padding,
        rest_bytes, rest_blocks, rest_allocs)

class PurifyAnalyze:
  ''' Given a Purify text file, parses all of the messages inside of it and
  normalizes them.  Provides a mechanism for comparing this normalized set
  against a baseline and detecting if new errors have been introduced. '''

  # a line which is the start of a new message
  pat_msg_start = re.compile('^\[([A-Z])\] (.*)$')
  # a message with a specific type
  pat_msg_type = re.compile('^([A-Z]{3}): (.*)$')
  pat_leak_summary = re.compile("Summary of ... memory leaks")
  pat_miu_summary = re.compile("Summary of ... memory in use")
  pat_starting = re.compile("Starting Purify'd ([^\\s]+\\\\[^\\s]+)")
  pat_arguments = re.compile("\s+Command line arguments:\s+([^\s].*)")
  pat_terminate = re.compile('Message: TerminateProcess called with code')
  # Purify treats this as a warning, but for us it's a fatal error.
  pat_instrumentation_failed = re.compile('^.* file not instrumented')
  # misc to ignore
  pat_ignore = (re.compile('^(Start|Exit)ing'),
                re.compile('^Program terminated'),
                re.compile('^Terminating thread'),
                re.compile('^Message: CryptVerifySignature'))
  # message types that aren't analyzed
  # handled, ignored and continued exceptions will likely never be interesting
  # TODO(erikkay): MPK ("potential" memory leaks) may be worth turning on
  types_excluded = ("EXH", "EXI", "EXC", "MPK")


  def __init__(self, files, echo, name=None, source_dir=None, data_dir=None):
    # The input file we're analyzing.
    self._files = files
    # Whether the input file contents should be echoed to stdout.
    self._echo = echo
    # A symbolic name for the run being analyzed, often the name of the
    # exe which was purified.
    self._name = name
    # The top of the source code tree of the code we're analyzing.
    # This prefix is stripped from all filenames in stacks for normalization.
    if source_dir:
      purify_message.Stack.SetSourceDir(source_dir)
    if data_dir:
      self._data_dir = data_dir
    else:
      self._data_dir = os.path.join(google.path_utils.ScriptDir(), "data")
    # A map of message_type to a MessageList of that type.
    self._message_lists = {}
    self._ReadIgnoreFile()

  def _ReadIgnoreFile(self):
    '''Read a file which is a list of regexps for either the title or the
    top-most visible stack line.
    '''
    self._pat_ignore = []
    filenames = [os.path.join(self._data_dir, "ignore.txt"),
        os.path.join(google.path_utils.ScriptDir(), "data", "ignore.txt")]
    for filename in filenames:
      if os.path.exists(filename):
        f = open(filename, 'r')
        for line in f.readlines():
          if line.startswith("#") or line.startswith("//") or line.isspace():
            continue
          line = line.rstrip()
          pat = re.compile(line)
          if pat:
            self._pat_ignore.append(pat)

  def ShouldIgnore(self, msg):
    '''Should the message be ignored as irrelevant to analysis '''
    # never ignore memory in use
    if msg.Type() == "MIU":
      return False

    # check ignore patterns against title and top-most visible stack frames
    strings = [msg._title]    
    err = msg.GetErrorStack()
    if err:
      line = err.GetTopVisibleStackLine().get('function', None)
      if line:
        strings.append(line)
    alloc = msg.GetAllocStack()
    if alloc:
      line = alloc.GetTopVisibleStackLine().get('function', None)
      if line:
        strings.append(line)
    for pat in self._pat_ignore:
      for str in strings:
        if pat.match(str):
          logging.debug("Igorning message based on ignore.txt")
          logging.debug(msg.NormalizedStr(verbose=True))
          return True

    # unless it's explicitly in the ignore file, never ignore these
    if msg.Type() == purify_message.FATAL:
      return False

    # certain message types aren't that interesting
    if msg.Type() in PurifyAnalyze.types_excluded:
      logging.debug("Igorning message because type is excluded")
      logging.debug(msg.NormalizedStr(verbose=True))
      return True
    # if the message stacks have no local stack frames, we can ignore them
    if msg.StacksAllExternal():
      logging.debug("Igorning message because stacks are all external")
      logging.debug(msg.NormalizedStr(verbose=True))
      return True

    # Microsoft's STL has a bunch of non-harmful UMRs in it.  Most of them
    # are filtered out by Purify's default filters and by our explicit ignore
    # list.  This code notices ones that have made it through so we can add
    # them to the ignore list later.
    if msg.Type() == "UMR":
      if err.GetTopStackLine()['file'].endswith('.'):
        logging.debug("non-ignored UMR in STL: %s" % msg._title)

    return False

  def AddMessage(self, msg):
    ''' Append the message to an array for its type.  Returns boolean indicating
    whether the message was actually added or was ignored.'''
    if msg:
      if self.ShouldIgnore(msg):
        return False
      if msg.Type() not in self._message_lists:
        self._message_lists[msg.Type()] = purify_message.MessageList(msg.Type())
      self._message_lists[msg.Type()].AddMessage(msg)
      return True
    return False

  def _BeginNewSublist(self, key):
    '''See MessageList.BeginNewSublist for details.
    '''
    if key in self._message_lists:
      self._message_lists[key].BeginNewSublist()

  def ReadFile(self):
    ''' Reads a Purify ASCII file and parses and normalizes the messages in
    the file.
    Returns False if a fatal error was detected, True otherwise.
    '''
    # Purify files consist of a series of "messages". These messages have a type
    # (designated as a three letter code - see message_type), a severity
    # (designated by a one letter code - see message_severity) and some
    # textual details.  It will often also have a stack trace of the error
    # location, and (for memory errors) may also have a stack trace of the
    # allocation location.

    fatal_errors = 0
    fatal_exe = ""

    for file in self._files:
      exe = ""
      error = None
      message = None
      for line in open(file, mode='rb'):
        line = line.rstrip()
        m = PurifyAnalyze.pat_msg_start.match(line)
        if m:
          if exe == fatal_exe:
            # since we hit a fatal error in this program, ignore all messages
            # until the program changes
            continue
          # we matched a new message, so if there's an existing one, it's time
          # to finish processing it
          if message:
            message.SetProgram(exe)
            if not self.AddMessage(message):
              # error is only set if the message we just tried to add would
              # otherwise be considered a fatal error.  Since AddMessage failed
              # (presumably the messages matched the ignore list), we reset
              # error to None
              error = None
          message = None
          if error:
            if error.Type() == "EXU":
              # Don't treat EXU as fatal, since unhandled exceptions
              # in other threads don't necessarily lead to the app to exit.
              # TODO(erikkay): verify that we still do trap exceptions that lead
              # to early termination.
              logging.warning(error.NormalizedStr(verbose=True))
              error = None
            else:
              if len(self._files) > 1:
                logging.error("Fatal error in program: %s" % error.Program())
              logging.error(error.NormalizedStr(verbose=True))
              fatal_errors += 1
              error = None
              fatal_exe = exe
              continue
          severity = m.group(1)
          line = m.group(2)
          m = PurifyAnalyze.pat_msg_type.match(line)
          if m:
            type = m.group(1)
            message = purify_message.Message(severity, type, m.group(2))
            if type == "EXU":
              error = message
          elif severity == "O":
            message = purify_message.Message(severity, purify_message.FATAL,
                                             line)
            # This is an internal Purify error, and it means that this run can't
            # be trusted and analysis should be aborted.
            error = message
          elif PurifyAnalyze.pat_instrumentation_failed.match(line):
            message = purify_message.Message(severity, purify_message.FATAL,
                                             line)
            error = message
          elif PurifyAnalyze.pat_terminate.match(line):
            message = purify_message.Message(severity, purify_message.FATAL,
                                             line)
            error = message
          elif PurifyAnalyze.pat_leak_summary.match(line):
            # TODO(erikkay): should we do sublists for MLK and MPK too?
            # Maybe that means we need to handle "new" and "all" messages
            # separately.
            #self._BeginNewSublist("MLK")
            #self._BeginNewSublist("MPK")
            pass
          elif PurifyAnalyze.pat_miu_summary.match(line):
            # Each time Purify is asked to do generate a list of all memory in use
            # or new memory in use, it first emits this summary line.  Since the
            # different lists can overlap, we need to tell MessageList to begin
            # a new sublist.
            # TODO(erikkay): should we tag "new" and "all" sublists explicitly
            # somehow?
            self._BeginNewSublist("MIU")
          elif PurifyAnalyze.pat_starting.match(line):
            m = PurifyAnalyze.pat_starting.match(line)
            exe = m.group(1)
            last_slash = exe.rfind("\\")
            if not purify_message.Stack.source_dir:
              path = os.path.abspath(os.path.join(exe[:last_slash], "..", ".."))
              purify_message.Stack.SetSourceDir(path)
            if not self._name:
              self._name = exe[(last_slash+1):]
          else:
            unknown = True
            for pat in PurifyAnalyze.pat_ignore:
              if pat.match(line):
                unknown = False
                break
            if unknown:
              logging.error("unknown line " + line)
        else:
          if message:
            message.AddLine(line)
          elif PurifyAnalyze.pat_arguments.match(line):
            m = PurifyAnalyze.pat_arguments.match(line)
            exe += " " + m.group(1)

      # Purify output should never end with a real message
      if message:
        logging.error("Unexpected message at end of file %s" % file)
  
    return fatal_errors == 0

  def GetMessageList(self, key):
    if key in self._message_lists:
      return self._message_lists[key]
    else:
      return None

  def PrintSummary(self, echo=None):
    ''' Print a summary of how many messages of each type were found. '''
    # make sure everyone else is done first
    sys.stderr.flush()
    sys.stdout.flush()
    if echo == None:
      echo = self._echo
    logging.info("summary of Purify messages:")
    self._ReportFixableMessages()
    for key in self._message_lists:
      list = self._message_lists[key]
      unique = list.UniqueMessages()
      all = list.AllMessages()
      count = 0
      for msg in all:
        count += msg._count
      logging.info("%s(%s) unique:%d total:%d" % (self._name, 
          purify_message.GetMessageType(key), len(unique), count))
      if key not in ["MIU"]:
        ignore_file = "%s_%s_ignore.txt" % (self._name, key)
        ignore_hashes = self._MessageHashesFromFile(ignore_file)
        ignored = 0
        
        groups = list.UniqueMessageGroups()
        group_keys = groups.keys()
        group_keys.sort(cmp=lambda x,y: len(groups[y]) - len(groups[x]))
        for group in group_keys:
          # filter out ignored messages
          kept_msgs= [x for x in groups[group] if hash(x) not in ignore_hashes]
          ignored += len(groups[group]) - len(kept_msgs)
          groups[group] = kept_msgs
        if ignored:
          logging.info("%s(%s) ignored:%d" % (self._name, 
            purify_message.GetMessageType(key), ignored))
        total = reduce(lambda x, y: x + len(groups[y]), group_keys, 0)
        if total:
          print "%s(%s) group summary:" % (self._name, 
            purify_message.GetMessageType(key))
          print "   TOTAL: %d" % total
          for group in group_keys:
            if len(groups[group]):
              print "   %s: %d" % (group, len(groups[group]))
        if echo:
          for group in group_keys:
            msgs = groups[group]
            if len(msgs) == 0:
              continue
            print "messages from %s (%d)" % (group, len(msgs))
            print "="*79
            for msg in msgs:
              # for the summary output, line numbers are useful
              print msg.NormalizedStr(verbose=True)
        # make sure stdout is flushed to avoid weird overlaps with logging
        sys.stdout.flush()

  def PrintMemoryInUse(self, byte_filter=16384):
    ''' Print one or more trees showing a hierarchy of memory allocations.
    Args:
      byte_filter: a number of bytes below which we'll prune the tree
    '''
    list = self.GetMessageList("MIU")
    sublists = list.GetSublists()
    if not sublists:
      sublists = [list]
    trees = []
    summaries = []
    # create the trees and summaries
    for sublist in sublists:
      tree = MemoryTreeNode.CreateTree(sublist)
      trees.append(tree)
      
      # while the tree is a hierarchical assignment from the root/bottom of the
      # stack down, the summary is simply adding the total of the top-most
      # stack item from our code
      summary = {}
      total = 0
      summaries.append(summary)
      for msg in sublist.AllMessages():
        total += msg._bytes
        stack = msg.GetAllocStack()
        if stack._all_external:
          alloc_caller = "WINDOWS"
        else:
          lines = stack.GetLines()
          for line in lines:
            alloc_caller = line["function"]
            if alloc_caller:
              break
        summary[alloc_caller] = summary.get(alloc_caller, 0) + msg._bytes
      summary["TOTAL"] = total

    # print out the summaries and trees.
    # TODO(erikkay): perhaps we should be writing this output to a file
    # instead?
    tree_number = 1
    num_trees = len(trees)
    for tree, summary in zip(trees, summaries):
      print "MEMORY SNAPSHOT %d of %d" % (tree_number, num_trees)
      lines = summary.keys()
      lines.sort(cmp=lambda x,y: summary[y] - summary[x])
      rest = 0
      for line in lines:
        bytes = summary[line]
        if bytes < byte_filter:
          rest += bytes
        else:
          print "%d: %s" % (bytes, line)
      print "%d: REST" % rest
      print
      print "BEGIN TREE"
      tree.PrintRecursive(byte_filter=byte_filter)
      tree_number += 1

    # make sure stdout is flushed to avoid weird overlaps with logging
    sys.stdout.flush()

  def PrintBugReport(self):
    ''' Print a summary of how many messages of each type were found. '''
    # make sure everyone else is done first
    sys.stderr.flush()
    sys.stdout.flush()
    logging.info("summary of Purify bugs:")
    # This is a specialized set of counters for layout tests, with some
    # unfortunate hard-coded knowledge.
    layout_test_counts = {}
    for key in self._message_lists:
      bug = {}
      list = self._message_lists[key]
      unique = list.UniqueMessages()
      all = list.AllMessages()
      count = 0
      for msg in all:
        if msg._title not in bug:
          # use a single sample message to represent all messages
          # that match this title
          bug[msg._title] = {"message":msg,
                             "total":0,
                             "count":0,
                             "programs":set()}
        this_bug = bug[msg._title]
        this_bug["total"] += msg._count
        this_bug["count"] += 1
        this_bug["programs"].add(msg.Program())
        # try to summarize the problem areas for layout tests
        if self._name == "layout":
          prog = msg.Program()
          prog_args = prog.split(" ")
          if len(prog_args):
            path = prog_args[-1].replace('\\', '/')
            index = path.rfind("layout_tests/")
            if index >= 0:
              path = path[(index + len("layout_tests/")):]
            else:
              index = path.rfind("127.0.0.1:")
              if index >= 0:
                # the port number is 8000 or 9000, but length is the same
                path = "http: " + path[(index + len("127.0.0.1:8000/")):]
            path = "/".join(path.split('/')[0:-1])
            count = 1 + layout_test_counts.get(path, 0)
            layout_test_counts[path] = count
      for title in bug:
        b = bug[title]
        print "[%s] %s" % (key, title)
        print "%d tests, %d stacks, %d instances" % (len(b["programs"]),
            b["count"], b["total"])
        print "Reproducible with:"
        for program in b["programs"]:
          print "   %s" % program
        print "Sample error details:"
        print "====================="
        print b["message"].NormalizedStr(verbose=True)
    if len(layout_test_counts):
      print
      print "Layout test error counts"
      print "========================"
      paths = layout_test_counts.keys()
      paths.sort()
      for path in paths:
        print "%s: %d" % (path, layout_test_counts[path])
    # make sure stdout is flushed to avoid weird overlaps with logging
    sys.stdout.flush()

  def SaveLatestStrings(self, string_list, key, fname_extra=""):
    '''Output a list of strings to a file in the "latest" dir.
    '''
    script_dir = google.path_utils.ScriptDir()
    path = os.path.join(script_dir, "latest")
    out = os.path.join(path, "%s_%s%s.txt" % (self._name, key, fname_extra))
    logging.info("saving %s" % (out))
    try:
      f = open(out, "w+")
      f.write('\n'.join(string_list))
    except IOError, (errno, strerror):
      logging.error("error writing to file %s (%d, %s)" % out, errno, strerror)
    if f:
      f.close()
    return True

  def SaveResults(self, path=None, verbose=False):
    ''' Output normalized data to baseline files for future comparison runs.
    Messages are saved in sorted order into a separate file for each message
    type.  See Message.NormalizedStr() for details of what's written.
    '''
    if not path:
      path = self._data_dir
    for key in self._message_lists:
      out = os.path.join(path, "%s_%s.txt" % (self._name, key))
      logging.info("saving %s" % (out))
      f = open(out, "w+")
      list = self._message_lists[key].UniqueMessages()
      # TODO(erikkay): should the count of each message be a diff factor?
      # (i.e. the same error shows up, but more frequently)
      for message in list:
        f.write(message.NormalizedStr(verbose=verbose))
        f.write("\n")
      f.close()
    return True

  def _ReportFixableMessages(self):
    ''' Collects all baseline files for the executable being tested, including
    lists of flakey results, and logs the total number of messages in them.
    '''
    # TODO(pamg): As long as we're looking at all the files, we could use the
    # information to report any message types that no longer happen at all.
    fixable = 0
    flakey = 0
    paths = [os.path.join(self._data_dir, x)
             for x in os.listdir(self._data_dir)]
    for path in paths:
      # We only care about this executable's files, and not its gtest filters.
      if (not os.path.basename(path).startswith(self._name) or
          not path.endswith(".txt") or
          path.endswith("gtest.txt") or
          not os.path.isfile(path)):
        continue
      msgs = self._MessageHashesFromFile(path)
      if path.find("flakey") == -1:
        fixable += len(msgs)
      else:
        flakey += len(msgs)

    logging.info("Fixable errors: %s" % fixable)
    logging.info("Flakey errors: %s" % flakey)

  def _MessageHashesFromFile(self, filename):
    ''' Reads a file of normalized messages (see SaveResults) and creates a
    dictionary mapping the hash of each message to its text.
    '''
    # NOTE: this uses the same hashing algorithm as Message.__hash__.
    # Unfortunately, we can't use the same code easily since Message is based
    # on parsing an original Purify output file and this code is reading a file
    # of already normalized messages.  This means that these two bits of code
    # need to be kept in sync.
    msgs = {}
    if not os.path.isabs(filename):
      filename = os.path.join(self._data_dir, filename)
    if os.path.exists(filename):
      logging.info("reading messages from %s" % filename)
      file = open(filename, "r")
      msg = ""
      title = None
      lines = file.readlines()
      # in case the file doesn't end in a blank line
      lines.append("\n")
      for line in lines:
        # allow these files to have comments in them
        if line.startswith('#') or line.startswith('//'):
          continue
        if not title:
          if not line.isspace():
            # first line of each message is a title
            title = line
          continue
        elif not line.isspace():
          msg += line
        else:
          # note that the hash doesn't include the title, see Message.__hash__
          h = hash(msg)
          msgs[h] = title + msg
          title = None
          msg = ""
      logging.info("%s: %d msgs" % (filename, len(msgs)))
    return msgs

  def _SaveLatestGroupSummary(self, message_list):
    '''Save a summary of message groups and their counts to a file in "latest"
    '''
    string_list = []
    groups = message_list.UniqueMessageGroups()
    group_keys = groups.keys()

    group_keys.sort(cmp=lambda x,y: len(groups[y]) - len(groups[x]))
    for group in group_keys:
      string_list.append("%s: %d" % (group, len(groups[group])))

    self.SaveLatestStrings(string_list, message_list.GetType(), "_GROUPS")

  def CompareResults(self):
    ''' Compares the results from the current run with the baseline data
    stored in data/<name>_<key>.txt returning False if it finds new errors
    that are not in the baseline.  See ReadFile() and SaveResults() for
    details of what's in the original file and what's in the baseline.
    Errors that show up in the baseline but not the current run are not
    considered errors (they're considered "fixed"), but they do suggest
    that the baseline file could be re-generated.'''
    errors = 0
    fixes = 0
    for type in purify_message.message_type:
      if type in ["MIU"]:
        continue
      # number of new errors for this message type
      type_errors = []
      # number of new unexpected fixes for this message type
      type_fixes = []
      # the messages from the current run that are in the baseline
      new_baseline = []
      # a common prefix used to describe the program being analyzed and the
      # type of message which is used to generate filenames and descriptive
      # error messages
      type_name = "%s_%s" % (self._name, type)
      
      # open the baseline file to compare against
      baseline_file = "%s.txt" % type_name
      baseline_hashes = self._MessageHashesFromFile(baseline_file)

      # read the flakey file if it exists
      flakey_file = "%s_flakey.txt" % type_name
      flakey_hashes = self._MessageHashesFromFile(flakey_file)

      # read the ignore file if it exists
      ignore_file = "%s_ignore.txt" % type_name
      ignore_hashes = self._MessageHashesFromFile(ignore_file)

      # messages from the current run
      current_list = self.GetMessageList(type)
      if current_list:
        # Since we're looking at the list of unique messages,
        # if the number of occurrances of a given unique message 
        # changes, it won't show up as an error.
        current_messages = current_list.UniqueMessages()
      else:
        current_messages = []
      current_hashes = {}
      # compute errors and new baseline
      for message in current_messages:
        msg_hash = hash(message)
        current_hashes[msg_hash] = message
        if msg_hash in ignore_hashes or msg_hash in flakey_hashes:
          continue
        if msg_hash in baseline_hashes:
          new_baseline.append(msg_hash)
          continue
        type_errors.append(msg_hash)
      # compute unexpected fixes
      for msg_hash in baseline_hashes:
        if (msg_hash not in current_hashes and
            msg_hash not in ignore_hashes and
            msg_hash not in flakey_hashes):
          type_fixes.append(baseline_hashes[msg_hash])

      if len(current_messages) or len(type_errors) or len(type_fixes):
        logging.info("%d '%s(%s)' messages "
                     "(%d new, %d unexpectedly fixed)" % (len(current_messages),
                     purify_message.GetMessageType(type), type,
                     len(type_errors), len(type_fixes)))

      if len(type_errors):
        strs = [current_hashes[x].NormalizedStr(verbose=True) 
                for x in type_errors]
        logging.error("%d new '%s(%s)' errors found\n%s" % (len(type_errors),
                      purify_message.GetMessageType(type), type, 
                      '\n'.join(strs)))
        strs = [current_hashes[x].NormalizedStr() for x in type_errors]
        self.SaveLatestStrings(strs, type, "_NEW")
        errors += len(type_errors)

      if len(type_fixes):
        # we don't have access to the original message, so all we can do is log
        # the non-verbose normalized text
        logging.warning("%d new '%s(%s)' unexpected fixes found\n%s" % (
                        len(type_fixes), purify_message.GetMessageType(type), 
                        type, '\n'.join(type_fixes)))
        self.SaveLatestStrings(type_fixes, type, "_FIXED")
        fixes += len(type_fixes)
        if len(current_messages) == 0:
          logging.warning("all errors fixed in %s" % baseline_file)

      if len(type_fixes) or len(type_errors):
        strs = [baseline_hashes[x] for x in new_baseline]
        self.SaveLatestStrings(strs, type, "_BASELINE")

      if current_list:
        self._SaveLatestGroupSummary(current_list)

    if errors:
      logging.error("%d total new errors found" % errors)
      return -1
    else:
      logging.info("no new errors found - yay!")
      if fixes:
        logging.warning("%d total errors unexpectedly fixed" % fixes)
        # magic return code to turn the builder orange (via ReturnCodeCommand)
        return -88
    return 0


# The following code is here for testing and development purposes.

def _main():
  retcode = 0

  parser = optparse.OptionParser("usage: %prog [options] <files to analyze>")
  parser.add_option("-b", "--baseline", action="store_true", default=False,
                    help="save output to baseline files")
  parser.add_option("-m", "--memory_in_use", 
                    action="store_true", default=False,
                    help="print memory in use summary")
  parser.add_option("", "--validate", 
                    action="store_true", default=False,
                    help="validate results vs. baseline")
  parser.add_option("-e", "--echo_to_stdout",
                    action="store_true", default=False,
                    help="echo purify output to standard output")
  parser.add_option("", "--source_dir",
                    help="path to top of source tree for this build"
                    "(used to normalize source paths in output)")
  parser.add_option("", "--byte_filter", default=16384,
                    help="prune the tree below this number of bytes")
  parser.add_option("-n", "--name",
                    help="name of the test being run "
                         "(used for output filenames)")
  parser.add_option("", "--data_dir",
                    help="path to where purify data files live")
  parser.add_option("", "--bug_report", default=False,
                    action="store_true",
                    help="print output as an attempted summary of bugs")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")

  (options, args) = parser.parse_args()
  if not len(args) >= 1:
    parser.error("no filename specified")
  filenames = args

  if options.verbose:
    google.logging_utils.config_root(level=logging.DEBUG)
  else:
    google.logging_utils.config_root(level=logging.INFO)
  pa = PurifyAnalyze(filenames, options.echo_to_stdout, options.name, 
                     options.source_dir, options.data_dir)
  execute_crash = not pa.ReadFile()
  if options.bug_report:
    pa.PrintBugReport()
    pa.PrintSummary(False)
  elif options.memory_in_use:
    pa.PrintMemoryInUse(int(options.byte_filter))
  elif execute_crash:
    retcode = -1
    logging.error("Fatal error during test execution.  Analysis skipped.")
  elif options.validate:
    if pa.CompareResults() != 0:
      retcode = -1
      script_dir = google.path_utils.ScriptDir()
      latest_dir = os.path.join(script_dir, "latest")
      pa.SaveResults(latest_dir)
    pa.PrintSummary()
  elif options.baseline:
    if not pa.SaveResults(verbose=True):
      retcode = -1
    pa.PrintSummary(False)
  else:
    pa.PrintSummary(False)

  sys.exit(retcode)

if __name__ == "__main__":
  _main()  


