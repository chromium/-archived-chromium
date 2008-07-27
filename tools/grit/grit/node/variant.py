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

'''The <skeleton> element.
'''


from grit.node import base


class SkeletonNode(base.Node):
  '''A <skeleton> element.'''
  
  # TODO(joi) Support inline skeleton variants as CDATA instead of requiring
  # a 'file' attribute.
  
  def MandatoryAttributes(self):
    return ['expr', 'variant_of_revision', 'file']
  
  def DefaultAttributes(self):
    '''If not specified, 'encoding' will actually default to the parent node's
    encoding.
    '''
    return {'encoding' : ''}

  def _ContentType(self):
    if self.attrs.has_key('file'):
      return self._CONTENT_TYPE_NONE
    else:
      return self._CONTENT_TYPE_CDATA
  
  def GetEncodingToUse(self):
    if self.attrs['encoding'] == '':
      return self.parent.attrs['encoding']
    else:
      return self.attrs['encoding']

  def GetFilePath(self):
    return self.ToRealPath(self.attrs['file'])
