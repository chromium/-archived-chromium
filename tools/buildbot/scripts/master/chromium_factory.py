#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


""" Custom BuildBot BuildFactory implementation source file. """

import buildbot.process.factory as factory
import chromium_process

class BuildFactory(factory.BuildFactory):
  """ Provides a way to specify our Build class implementation.

  Another alternative to using this class is to set buildClass attribute
  directly with the build class on the  buildbot.process.factory.BuildFactory
  object. However this approach will keep it encapsulated.
  """
  buildClass = chromium_process.Build
