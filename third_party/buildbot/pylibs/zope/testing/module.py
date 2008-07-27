##############################################################################
#
# Copyright (c) 2004 Zope Corporation and Contributors.
# All Rights Reserved.
#
# This software is subject to the provisions of the Zope Public License,
# Version 2.1 (ZPL).  A copy of the ZPL should accompany this distribution.
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY AND ALL EXPRESS OR IMPLIED
# WARRANTIES ARE DISCLAIMED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF TITLE, MERCHANTABILITY, AGAINST INFRINGEMENT, AND FITNESS
# FOR A PARTICULAR PURPOSE.
#
##############################################################################
"""Fake module support

$Id: module.py 28956 2005-01-25 17:32:17Z benji_york $
"""

import sys

class FakeModule:
    def __init__(self, dict):
        self.__dict = dict
    def __getattr__(self, name):
        try:
            return self.__dict[name]
        except KeyError:
            raise AttributeError, name

def setUp(test, name='README.txt'):
    dict = test.globs
    dict['__name__'] = name    
    sys.modules[name] = FakeModule(dict)

def tearDown(test, name='README.txt'):
    del sys.modules[name]
