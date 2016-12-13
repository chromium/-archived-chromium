#!/usr/bin/python2.4
#
# Copyright 2007 Google Inc. All Rights Reserved.

"""Imports a set of drivers appropriate to the current OS."""

__author__ = 'jhaas@google.com (Jonathan Haas)'

import sys

platform_dir = sys.platform

keyboard  = __import__(platform_dir+".keyboard",  globals(), locals(), [''])
mouse     = __import__(platform_dir+".mouse",     globals(), locals(), [''])
windowing = __import__(platform_dir+".windowing", globals(), locals(), [''])
