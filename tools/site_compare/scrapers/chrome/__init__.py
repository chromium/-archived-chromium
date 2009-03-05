#!/usr/bin/python2.4
#
# Copyright 2007 Google Inc. All Rights Reserved.

"""Selects the appropriate scraper for Chrome."""

__author__ = 'jhaas@google.com (Jonathan Haas)'

def GetScraper(version):
  """Returns the scraper module for the given version.

  Args:
    version: version string of Chrome, or None for most recent

  Returns:
    scrape module for given version
  """
  if version is None:
    version = "0.1.101.0"

  parsed_version = [int(x) for x in version.split(".")]

  if (parsed_version[0] > 0 or
      parsed_version[1] > 1 or
      parsed_version[2] > 97 or
      parsed_version[3] > 0):
    scraper_version = "chrome011010"
  else:
    scraper_version = "chrome01970"

  return __import__(scraper_version, globals(), locals(), [''])

# if invoked rather than imported, test
if __name__ == "__main__":
  version = "0.1.101.0"

  print GetScraper(version).version

