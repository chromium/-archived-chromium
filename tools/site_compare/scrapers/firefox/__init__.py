#!/usr/bin/python2.4
#
# Copyright 2007 Google Inc. All Rights Reserved.

"""Selects the appropriate scraper for Firefox."""

__author__ = 'jhaas@google.com (Jonathan Haas)'


def GetScraper(version):
  """Returns the scraper module for the given version.

  Args:
    version: version string of IE, or None for most recent

  Returns:
    scrape module for given version
  """

  # Pychecker will warn that the parameter is unused; we only
  # support one version of Firefox at this time

  # We only have one version of the Firefox scraper for now
  return __import__("firefox2", globals(), locals(), [''])

# if invoked rather than imported, test
if __name__ == "__main__":
  version = "2.0.0.6"

  print GetScraper("2.0.0.6").version

