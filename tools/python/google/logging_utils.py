#!/bin/env python
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

# logging_utils.py

''' Utility functions and objects for logging.
'''

import logging
import sys

class StdoutStderrHandler(logging.Handler):
  ''' Subclass of logging.Handler which outputs to either stdout or stderr
  based on a threshold level.
  '''

  def __init__(self, threshold=logging.WARNING, err=sys.stderr, out=sys.stdout):
    ''' Args:
          threshold: below this logging level messages are sent to stdout,
            otherwise they are sent to stderr
          err: a stream object that error messages are sent to, defaults to
            sys.stderr
          out: a stream object that non-error messages are sent to, defaults to
            sys.stdout
    '''
    logging.Handler.__init__(self)
    self._err = logging.StreamHandler(err)    
    self._out = logging.StreamHandler(out)
    self._threshold = threshold
    self._last_was_err = False
    
  def setLevel(self, lvl):
    logging.Handler.setLevel(self, lvl)
    self._err.setLevel(lvl)
    self._out.setLevel(lvl)

  def setFormatter(self, formatter):
    logging.Handler.setFormatter(self, formatter)
    self._err.setFormatter(formatter)
    self._out.setFormatter(formatter)

  def emit(self, record):
    if record.levelno < self._threshold:
      self._out.emit(record)
      self._last_was_err = False
    else:
      self._err.emit(record)
      self._last_was_err = False

  def flush(self):
    # preserve order on the flushing, the stalest stream gets flushed first
    if self._last_was_err:
      self._out.flush()
      self._err.flush()
    else:
      self._err.flush()
      self._out.flush()


FORMAT = "%(asctime)s %(filename)s [%(levelname)s] %(message)s"
DATEFMT = "%H:%M:%S"

def config_root(level=logging.INFO, threshold=logging.WARNING, format=FORMAT, 
         datefmt=DATEFMT):
  ''' Configure the root logger to use a StdoutStderrHandler and some default
  formatting. 
    Args:
      level: messages below this level are ignored
      threshold: below this logging level messages are sent to stdout,
        otherwise they are sent to stderr
      format: format for log messages, see logger.Format
      datefmt: format for date in log messages
      
  '''
  # to set the handler of the root logging object, we need to do setup
  # manually rather than using basicConfig
  root = logging.getLogger()
  root.setLevel(level)
  formatter = logging.Formatter(format, datefmt)
  handler = StdoutStderrHandler(threshold=threshold)
  handler.setLevel(level)
  handler.setFormatter(formatter)
  root.addHandler(handler)
