#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit build' tool along with integration for this tool with the
SCons build system.
'''

import os
import getopt
import types
import sys

from grit import grd_reader
from grit import util
from grit.tool import interface
from grit import shortcuts


def ParseDefine(define):
  '''Parses a define that is either like "NAME" or "NAME=VAL" and
  returns its components, using True as the default value.  Values of
  "1" and "0" are transformed to True and False respectively.
  '''
  parts = [part.strip() for part in define.split('=')]
  assert len(parts) >= 1
  name = parts[0]
  val = True
  if len(parts) > 1:
    val = parts[1]
  if val == "1": val = True
  elif val == "0": val = False
  return (name, val)


class RcBuilder(interface.Tool):
  '''A tool that builds RC files and resource header files for compilation.

Usage:  grit build [-o OUTPUTDIR] [-D NAME[=VAL]]*

All output options for this tool are specified in the input file (see
'grit help' for details on how to specify the input file - it is a global
option).

Options:

  -o OUTPUTDIR      Specify what directory output paths are relative to.
                    Defaults to the current directory.
  
  -D NAME[=VAL]     Specify a C-preprocessor-like define NAME with optional
                    value VAL (defaults to 1) which will be used to control
                    conditional inclusion of resources.

Conditional inclusion of resources only affects the output of files which
control which resources get linked into a binary, e.g. it affects .rc files
meant for compilation but it does not affect resource header files (that define
IDs).  This helps ensure that values of IDs stay the same, that all messages
are exported to translation interchange files (e.g. XMB files), etc.
'''

  def ShortDescription(self):
    return 'A tool that builds RC files for compilation.'

  def Run(self, opts, args):
    self.output_directory = '.'
    (own_opts, args) = getopt.getopt(args, 'o:D:')
    for (key, val) in own_opts:
      if key == '-o':
        self.output_directory = val
      elif key == '-D':
        name, val = ParseDefine(val)
        self.defines[name] = val
    if len(args):
      print "This tool takes no tool-specific arguments."
      return 2
    self.SetOptions(opts)
    if self.scons_targets:
      self.VerboseOut('Using SCons targets to identify files to output.\n')
    else:
      self.VerboseOut('Output directory: %s (absolute path: %s)\n' %
                      (self.output_directory,
                       os.path.abspath(self.output_directory)))
    self.res = grd_reader.Parse(opts.input, debug=opts.extra_verbose)
    self.res.RunGatherers(recursive = True)
    self.Process()
    return 0

  def __init__(self):
    # Default file-creation function is built-in file().  Only done to allow
    # overriding by unit test.
    self.fo_create = file
    
    # key/value pairs of C-preprocessor like defines that are used for
    # conditional output of resources
    self.defines = {}

    # self.res is a fully-populated resource tree if Run()
    # has been called, otherwise None.
    self.res = None
    
    # Set to a list of filenames for the output nodes that are relative
    # to the current working directory.  They are in the same order as the
    # output nodes in the file.
    self.scons_targets = None
  
  # static method
  def ProcessNode(node, output_node, outfile):
    '''Processes a node in-order, calling its formatter before and after
    recursing to its children.
    
    Args:
      node: grit.node.base.Node subclass
      output_node: grit.node.io.File
      outfile: open filehandle
    '''
    base_dir = util.dirname(output_node.GetOutputFilename())
    
    try:
      formatter = node.ItemFormatter(output_node.GetType())
      if formatter:
        outfile.write(formatter.Format(node, output_node.GetLanguage(),
                                       begin_item=True, output_dir=base_dir))
    except:
      print u"Error processing node %s" % unicode(node)
      raise

    for child in node.children:
      RcBuilder.ProcessNode(child, output_node, outfile)

    try:
      if formatter:
        outfile.write(formatter.Format(node, output_node.GetLanguage(),
                                       begin_item=False, output_dir=base_dir))
    except:
      print u"Error processing node %s" % unicode(node)
      raise
  ProcessNode = staticmethod(ProcessNode)


  def Process(self):
    # Update filenames with those provided by SCons if we're being invoked
    # from SCons.  The list of SCons targets also includes all <structure>
    # node outputs, but it starts with our output files, in the order they
    # occur in the .grd
    if self.scons_targets:
      assert len(self.scons_targets) >= len(self.res.GetOutputFiles())
      outfiles = self.res.GetOutputFiles()
      for ix in range(len(outfiles)):
        outfiles[ix].output_filename = os.path.abspath(
          self.scons_targets[ix])
    else:
      for output in self.res.GetOutputFiles():
        output.output_filename = os.path.abspath(os.path.join(
          self.output_directory, output.GetFilename()))
    
    for output in self.res.GetOutputFiles():
      self.VerboseOut('Creating %s...' % output.GetFilename())

      # Don't build data package files on mac/windows because it's not used and
      # there are project dependency issues.  We still need to create the file
      # to satisfy build dependencies.
      if output.GetType() == 'data_package' and sys.platform != 'linux2':
        f = open(output.GetOutputFilename(), 'wb')
        f.close()
        continue

      # Microsoft's RC compiler can only deal with single-byte or double-byte
      # files (no UTF-8), so we make all RC files UTF-16 to support all
      # character sets.
      if output.GetType() in ['rc_header']:
        encoding = 'cp1252'
      else:
        encoding = 'utf_16'
      outfile = self.fo_create(output.GetOutputFilename(), 'wb')

      if output.GetType() != 'data_package':
        outfile = util.WrapOutputStream(outfile, encoding)
      
      # Set the context, for conditional inclusion of resources
      self.res.SetOutputContext(output.GetLanguage(), self.defines)
      
      # TODO(joi) Handle this more gracefully
      import grit.format.rc_header
      grit.format.rc_header.Item.ids_ = {}
      
      # Iterate in-order through entire resource tree, calling formatters on
      # the entry into a node and on exit out of it.
      self.ProcessNode(self.res, output, outfile)
      
      outfile.close()
      self.VerboseOut(' done.\n')
    
    # Print warnings if there are any duplicate shortcuts.
    print '\n'.join(shortcuts.GenerateDuplicateShortcutsWarnings(
      self.res.UberClique(), self.res.GetTcProject()))
    
    # Print out any fallback warnings, and missing translation errors, and
    # exit with an error code if there are missing translations in a non-pseudo
    # build
    print (self.res.UberClique().MissingTranslationsReport().
        encode('ascii', 'replace'))
    if self.res.UberClique().HasMissingTranslations():
      sys.exit(-1)

