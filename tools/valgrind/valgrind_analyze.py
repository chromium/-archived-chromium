#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# valgrind_analyze.py

''' Given a valgrind XML file, parses errors and uniques them.'''

import logging
import optparse
import os
import sys
import time
from xml.dom.minidom import parse

# These are functions (using C++ mangled names) that we look for in stack
# traces. We don't show stack frames while pretty printing when they are below
# any of the following:
_TOP_OF_STACK_POINTS = [
  # Don't show our testing framework.
  "testing::Test::Run()",
  # Also don't show the internals of libc/pthread.
  "start_thread"
]

def getTextOf(top_node, name):
  ''' Returns all text in all DOM nodes with a certain |name| that are children
  of |top_node|.
  '''

  text = ""
  for nodes_named in top_node.getElementsByTagName(name):
    text += "".join([node.data for node in nodes_named.childNodes
                     if node.nodeType == node.TEXT_NODE])
  return text

def removeCommonRoot(source_dir, directory):
  '''Returns a string with the string prefix |source_dir| removed from
  |directory|.'''
  if source_dir:
    # Do this for safety, just in case directory is an absolute path outside of
    # source_dir.
    prefix = os.path.commonprefix([source_dir, directory])
    return directory[len(prefix) + 1:]

  return directory

# Constants that give real names to the abbreviations in valgrind XML output.
INSTRUCTION_POINTER = "ip"
OBJECT_FILE = "obj"
FUNCTION_NAME = "fn"
SRC_FILE_DIR = "dir"
SRC_FILE_NAME = "file"
SRC_LINE = "line"

def gatherFrames(node, source_dir):
  frames = []
  for frame in node.getElementsByTagName("frame"):
    frame_dict = {
      INSTRUCTION_POINTER : getTextOf(frame, INSTRUCTION_POINTER),
      OBJECT_FILE         : getTextOf(frame, OBJECT_FILE),
      FUNCTION_NAME       : getTextOf(frame, FUNCTION_NAME),
      SRC_FILE_DIR        : removeCommonRoot(
          source_dir, getTextOf(frame, SRC_FILE_DIR)),
      SRC_FILE_NAME       : getTextOf(frame, SRC_FILE_NAME),
      SRC_LINE            : getTextOf(frame, SRC_LINE)
    }
    frames += [frame_dict]
    if frame_dict[FUNCTION_NAME] in _TOP_OF_STACK_POINTS:
      break
  return frames

class ValgrindError:
  ''' Takes a <DOM Element: error> node and reads all the data from it. A
  ValgrindError is immutable and is hashed on its pretty printed output.
  '''

  def __init__(self, source_dir, error_node):
    ''' Copies all the relevant information out of the DOM and into object
    properties.

    Args:
      error_node: The <error></error> DOM node we're extracting from.
      source_dir: Prefix that should be stripped from the <dir> node.
    '''

    # Valgrind errors contain one <what><stack> pair, plus an optional
    # <auxwhat><stack> pair, plus an optional <origin><what><stack></origin>.
    # (Origin is nicely enclosed; too bad the other two aren't.)
    # The most common way to see all three in one report is
    # a syscall with a parameter that points to uninitialized memory, e.g.
    # Format:
    # <error>
    #   <unique>0x6d</unique>
    #   <tid>1</tid>
    #   <kind>SyscallParam</kind>
    #   <what>Syscall param write(buf) points to uninitialised byte(s)</what>
    #   <stack>
    #     <frame>
    #     ...
    #     </frame>
    #   </stack>
    #   <auxwhat>Address 0x5c9af4f is 7 bytes inside a block of ...</auxwhat>
    #   <stack>
    #     <frame>
    #     ...
    #     </frame>
    #   </stack>
    #   <origin>
    #   <what>Uninitialised value was created by a heap allocation</what>
    #   <stack>
    #     <frame>
    #     ...
    #     </frame>
    #   </stack>
    #   </origin>

    self._kind = getTextOf(error_node, "kind")
    self._backtraces = []

    # Iterate through the nodes, parsing <what|auxwhat><stack> pairs.
    description = None
    for node in error_node.childNodes:
      if node.localName == "what" or node.localName == "auxwhat":
        description = "".join([n.data for n in node.childNodes
                              if n.nodeType == n.TEXT_NODE])
      elif node.localName == "stack":
        self._backtraces.append([description, gatherFrames(node, source_dir)])
        description = None
      elif node.localName == "origin":
        description = getTextOf(node, "what")
        stack = node.getElementsByTagName("stack")[0]
        frames = gatherFrames(stack, source_dir)
        self._backtraces.append([description, frames])
        description = None
        stack = None
        frames = None

  def __str__(self):
    ''' Pretty print the type and backtrace(s) of this specific error.'''
    output = self._kind + "\n"
    for backtrace in self._backtraces:
      output += backtrace[0] + "\n"
      for frame in backtrace[1]:
        output += ("  " + (frame[FUNCTION_NAME] or frame[INSTRUCTION_POINTER]) +
                   " (")

        if frame[SRC_FILE_DIR] != "":
          output += (frame[SRC_FILE_DIR] + "/" + frame[SRC_FILE_NAME] + ":" +
                     frame[SRC_LINE])
        else:
          output += frame[OBJECT_FILE]
        output += ")\n"

    return output

  def UniqueString(self):
    ''' String to use for object identity. Don't print this, use str(obj)
    instead.'''
    rep = self._kind + " "
    for backtrace in self._backtraces:
      for frame in backtrace[1]:
        rep += frame[FUNCTION_NAME]

        if frame[SRC_FILE_DIR] != "":
          rep += frame[SRC_FILE_DIR] + "/" + frame[SRC_FILE_NAME]
        else:
          rep += frame[OBJECT_FILE]

    return rep

  def __hash__(self):
    return hash(self.UniqueString())
  def __eq__(self, rhs):
    return self.UniqueString() == rhs

class ValgrindAnalyze:
  ''' Given a set of Valgrind XML files, parse all the errors out of them,
  unique them and output the results.'''

  def __init__(self, source_dir, files, show_all_leaks=False):
    '''Reads in a set of files.

    Args:
      source_dir: Path to top of source tree for this build
      files: A list of filenames.
      show_all_leaks: whether to show even less important leaks
    '''

    self._errors = set()
    for file in files:
      # Wait up to a minute for valgrind to finish writing.
      f = open(file, "r")
      ntries = 60
      for tries in range(0, ntries):
        f.seek(0)
        if sum((1 for line in f if '</valgrindoutput>' in line)) > 0:
          break
        time.sleep(1)
      f.close()
      if tries == ntries - 1:
        logging.error("valgrind never finished?")
      raw_errors = parse(file).getElementsByTagName("error")
      for raw_error in raw_errors:
        # Ignore "possible" leaks for now by default.
        if (show_all_leaks or
            getTextOf(raw_error, "kind") != "Leak_PossiblyLost"):
          self._errors.add(ValgrindError(source_dir, raw_error))

  def Report(self):
    if self._errors:
      logging.error("FAIL! There were %s errors: " % len(self._errors))

      for error in self._errors:
        logging.error(error)

      return -1

    logging.info("PASS! No errors found!")
    return 0

def _main():
  '''For testing only. The ValgrindAnalyze class should be imported instead.'''
  retcode = 0
  parser = optparse.OptionParser("usage: %prog [options] <files to analyze>")
  parser.add_option("", "--source_dir",
                    help="path to top of source tree for this build"
                    "(used to normalize source paths in baseline)")

  (options, args) = parser.parse_args()
  if not len(args) >= 1:
    parser.error("no filename specified")
  filenames = args

  analyzer = ValgrindAnalyze(options.source_dir, filenames)
  retcode = analyzer.Report()

  sys.exit(retcode)

if __name__ == "__main__":
  _main()
