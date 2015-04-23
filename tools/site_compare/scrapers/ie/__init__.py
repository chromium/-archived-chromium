#!/usr/bin/python2.4
#
# Copyright 2007 Google Inc. All Rights Reserved.

"""Selects the appropriate scraper for Internet Explorer."""

__author__ = 'jhaas@google.com (Jonathan Haas)'


def GetScraper(version):
  """Returns the scraper module for the given version.

  Args:
    version: version string of IE, or None for most recent

  Returns:
    scrape module for given version
  """

  # Pychecker will warn that the parameter is unused; we only
  # support one version of IE at this time

  # We only have one version of the IE scraper for now
  return __import__("ie7", globals(), locals(), [''])

# if invoked rather than imported, test
if __name__ == "__main__":
  version = "7.0.5370.1"

  print GetScraper(version).version

