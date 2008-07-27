#!/usr/bin/python2.4
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

'''Base class and interface for tools.
'''

import sys

class Tool(object):
  '''Base class for all tools.  Tools should use their docstring (i.e. the
  class-level docstring) for the help they want to have printed when they
  are invoked.'''

  #
  # Interface (abstract methods)
  #

  def ShortDescription(self):
    '''Returns a short description of the functionality of the tool.'''
    raise NotImplementedError()
    
  def Run(self, global_options, my_arguments):
    '''Runs the tool.
    
    Args:
      global_options: object grit_runner.Options
      my_arguments: [arg1 arg2 ...]
    
    Return:
      0 for success, non-0 for error
    '''
    raise NotImplementedError()

  #
  # Base class implementation
  #
  
  def __init__(self):
    self.o = None
  
  def SetOptions(self, opts):
    self.o = opts
  
  def Out(self, text):
    '''Always writes out 'text'.'''
    self.o.output_stream.write(text)
  
  def VerboseOut(self, text):
    '''Writes out 'text' if the verbose option is on.'''
    if self.o.verbose:
      self.o.output_stream.write(text)
  
  def ExtraVerboseOut(self, text):
    '''Writes out 'text' if the extra-verbose option is on.
    '''
    if self.o.extra_verbose:
      self.o.output_stream.write(text)