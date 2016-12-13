#!/usr/bin/python2.4
#
# Copyright 2007 Google Inc. All Rights Reserved.

"""Selects the appropriate scraper for a given browser and version."""

__author__ = 'jhaas@google.com (Jonathan Haas)'

import types

# TODO(jhaas): unify all optional scraper parameters into kwargs

def GetScraper(browser):
  """Given a browser and an optional version, returns the scraper module.

  Args:
    browser: either a string (browser name) or a tuple (name, version)

  Returns:
    module
  """

  if type(browser) == types.StringType: browser = (browser, None)

  package = __import__(browser[0], globals(), locals(), [''])
  module = package.GetScraper(browser[1])
  if browser[1] is not None: module.version = browser[1]

  return module

# if invoked rather than imported, do some tests
if __name__ == "__main__":
  print GetScraper("IE")

