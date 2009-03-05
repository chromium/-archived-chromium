#!/usr/bin/python2.4
#
# Copyright 2007 Google Inc. All Rights Reserved.

"""Selects the appropriate operator."""

__author__ = 'jhaas@google.com (Jonathan Haas)'


def GetOperator(operator):
  """Given an operator by name, returns its module.

  Args:
    operator: string describing the comparison

  Returns:
    module
  """

  # TODO(jhaas): come up with a happy way of integrating multiple operators
  # with different, possibly divergent and possibly convergent, operators.

  module = __import__(operator, globals(), locals(), [''])

  return module

