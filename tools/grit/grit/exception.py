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

'''Exception types for GRIT.
'''

class Base(Exception):
  '''A base exception that uses the class's docstring in addition to any
  user-provided message as the body of the Base.
  '''
  def __init__(self, msg=''):
    if len(msg):
      if self.__doc__:
        msg = self.__doc__ + ': ' + msg
    else:
      msg = self.__doc__
    Exception.__init__(self, msg)


class Parsing(Base):
  '''An error occurred parsing a GRD or XTB file.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class UnknownElement(Parsing):
  '''An unknown node type was encountered.'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class MissingElement(Parsing):
  '''An expected element was missing.'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class UnexpectedChild(Parsing):
  '''An unexpected child element was encountered (on a leaf node).'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class UnexpectedAttribute(Parsing):
  '''The attribute was not expected'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class UnexpectedContent(Parsing):
  '''This element should not have content'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class MissingMandatoryAttribute(Parsing):
  '''This element is missing a mandatory attribute'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class MutuallyExclusiveMandatoryAttribute(Parsing):
  '''This element has 2 mutually exclusive mandatory attributes'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class DuplicateKey(Parsing):
  '''A duplicate key attribute was found.'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class TooManyExamples(Parsing):
  '''Only one <ex> element is allowed for each <ph> element.'''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class GotPathExpectedFilenameOnly(Parsing):
  '''The 'filename' attribute of an <output> node must not be a path, only
  a filename.
  '''
  def __init__(self, msg=''):
    Parsing.__init__(self, msg)


class InvalidMessage(Base):
  '''The specified message failed validation.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class InvalidTranslation(Base):
  '''Attempt to add an invalid translation to a clique.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class NoSuchTranslation(Base):
  '''Requested translation not available'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class NotReady(Base):
  '''Attempt to use an object before it is ready, or attempt to translate
  an empty document.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class TooManyPlaceholders(Base):
  '''Too many placeholders for elements of the same type.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class MismatchingPlaceholders(Base):
  '''Placeholders do not match.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class InvalidPlaceholderName(Base):
  '''Placeholder name can only contain A-Z, a-z, 0-9 and underscore.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class BlockTagInTranslateableChunk(Base):
  '''A block tag was encountered where it wasn't expected.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class SectionNotFound(Base):
  '''The section you requested was not found in the RC file. Make
sure the section ID is correct (matches the section's ID in the RC file).
Also note that you may need to specify the RC file's encoding (using the
encoding="" attribute) if it is not in the default Windows-1252 encoding.
  '''
  def __init__(self, msg=''):
    Base.__init__(self, msg)


class IdRangeOverlap(Base):
  '''ID range overlap.'''
  def __init__(self, msg=''):
    Base.__init__(self, msg)
