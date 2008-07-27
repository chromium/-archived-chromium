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

'''Interface for all gatherers.
'''


from grit import clique


class GathererBase(object):
  '''Interface for all gatherer implementations.  Subclasses must implement
  all methods that raise NotImplemented.'''

  def __init__(self):
    # A default uberclique that is local to this object.  Users can override
    # this with the uberclique they are using.
    self.uberclique = clique.UberClique()
    # Indicates whether this gatherer is a skeleton gatherer, in which case
    # we should not do some types of processing on the translateable bits.
    self.is_skeleton = False

  def SetUberClique(self, uberclique):
    '''Overrides the default uberclique so that cliques created by this object
    become part of the uberclique supplied by the user.
    '''
    self.uberclique = uberclique
  
  def SetSkeleton(self, is_skeleton):
    self.is_skeleton = is_skeleton
  
  def IsSkeleton(self):
    return self.is_skeleton
  
  def Parse(self):
    '''Parses the contents of what is being gathered.'''
    raise NotImplementedError()
  
  def GetText(self):
    '''Returns the text of what is being gathered.'''
    raise NotImplementedError()
  
  def GetTextualIds(self):
    '''Returns the mnemonic IDs that need to be defined for the resource
    being gathered to compile correctly.'''
    return []
    
  def GetCliques(self):
    '''Returns the MessageClique objects for all translateable portions.'''
    return []
  
  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    '''Returns the resource being gathered, with translateable portions filled
    with the translation for language 'lang'.
    
    If pseudo_if_not_available is true, a pseudotranslation will be used for any
    message that doesn't have a real translation available.
    
    If no translation is available and pseudo_if_not_available is false,
    fallback_to_english controls the behavior.  If it is false, throw an error.
    If it is true, use the English version of the message as its own
    "translation".
    
    If skeleton_gatherer is specified, the translation will use the nontranslateable
    parts from the gatherer 'skeleton_gatherer', which must be of the same type
    as 'self'.
    
    If fallback_to_english 
    
    Args:
      lang: 'en'
      pseudo_if_not_available: True | False
      skeleton_gatherer: other_gatherer
      fallback_to_english: True | False
    
    Return:
      e.g. 'ID_THIS_SECTION TYPE\n...BEGIN\n  "Translated message"\n......\nEND'
    
    Raises:
      grit.exception.NotReady() if used before Parse() has been successfully
      called.
      grit.exception.NoSuchTranslation() if 'pseudo_if_not_available' and
      fallback_to_english are both false and there is no translation for the
      requested language.
    '''
    raise NotImplementedError()
  
  def FromFile(rc_file, extkey=None, encoding = 'cp1252'):
    '''Loads the resource from the file 'rc_file'.  Optionally an external key
    (which gets passed to the gatherer's constructor) can be specified.
    
    If 'rc_file' is a filename, it will be opened for reading using 'encoding'.
    Otherwise the 'encoding' parameter is ignored.
    
    Args:
      rc_file: file('') | 'filename.rc'
      extkey: e.g. 'ID_MY_DIALOG'
      encoding: 'utf-8'
    
    Return:
      grit.gather.interface.GathererBase subclass
    '''
    raise NotImplementedError()
  FromFile = staticmethod(FromFile)
