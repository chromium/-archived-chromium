#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""General JSON data file and list- and dict-specific subclasses."""

import copy
import errno
import os
import simplejson
import sys

from twisted.python import log

ENCODING = 'utf8'

class JSONDataFile(object):
  """General, semi-shared JSON data file for non-critical data.

  Data stored by this class must be both copyable (using copy.copy()) and
  serializable using simplejson.  Strings will be saved and loaded as Unicode.

  The file created may be shared among several processes, but if it is, there
  is some risk of data loss.  An attempt has been made to balance the risk of
  loss with the expense of disk operations.  Specifically,
    - Data read (with Data()) will be stale if other processes have been
      modifying the file, unless fresh data is requested.
    - The file will be re-loaded before being modified, but no lock is used,
      so a race condition at the wrong moment may cause loss of data.

  Nevertheless, this class is suitable either for non-shared files or for data
  that's not very important.
  """
  def __init__(self, filename, default, mode=None, varname=None):
    """Initialize the class and load the file.
    Args:
      filename: full path to the file to load or create
      default: default data to use if the file does not exist
      mode: if given, the numeric Unix file mode to give the file (e.g., 0644).
          If absent or None, the current umask will be used implicitly.
          (os.chmod doesn't work in Windows, so this doesn't either.)
      varname: if present, the variable name to assign to the JSON object
          written. This turns the JSON file into a complete JavaScript snippet.
          For example, if you're storing the Python dict {'a':1, 'b':2},
            if varname=None, the file will contain  {"a": 1, "b": 2}
            if varname=x,    the file will contain  x = {"a": 1, "b": 2}
    """
    self._filename = filename
    self._data = default
    if mode is None:
      self._mode = None
    else:
      self._mode = int(mode)
    if varname:
      self._prefix = 'var %s = ' % varname
    else:
      self._prefix = None
    self._Load()

  def _Load(self):
    """Loads data from the JSON file, overwriting the local copy if any.

    If the file doesn't exist or can't be parsed, keep the previous local data.
    """
    f = None
    json_data = None
    try:
      f = open(self._filename, 'r')
      json_data = f.read()
    except IOError, e:
      log.msg('Loading in %s: %s' % (os.getcwd(), e))
    if f:
      f.close()
    if json_data:
      if self._prefix and json_data.startswith(self._prefix):
        json_data = json_data[len(self._prefix):]
      self._data = simplejson.loads(json_data, encoding=ENCODING)

  def _Save(self):
    """Saves local data into the JSON file, optionally prepending the prefix.

    If the object can't be serialized or the file can't be written, leaves the
    previous file unchanged.

    A temporary file named with self._filename + '_new' is used to make the
    save more atomic. Note that this is a potential security hole.
    """
    if self._prefix:
      json_data = self._prefix
    else:
      json_data = ''
    json_data += simplejson.dumps(self._data, encoding=ENCODING)
    new_filename = self._filename + '_new'
    f = None
    try:
      f = open(new_filename, 'w')
      f.write(json_data)
    except IOError, e:
      log.msg('Saving in %s: %s' % (os.getcwd(), e))
    if f:
      f.close()
    if sys.platform == 'win32':
      # Windows won't rename on top of an existing file. The bot runs in
      # Linux, but this makes testing easier.
      try:
        os.remove(self._filename)
      except OSError, e:
        if e.errno == errno.ENOENT:  # No such file
          pass
    try:
      os.rename(new_filename, self._filename)
      if self._mode is not None:
        os.chmod(self._filename, self._mode)
    except OSError, e:
      log.msg('Renaming/setting mode in %s: %s' % (os.getcwd(), e))

  def Reset(self, data):
    """Sets the local data to a copy of data, then saves it to the file.

    When modifying existing data, callers will probably want to load a fresh
    copy before resetting it, to avoid data loss.
    """
    self._data = copy.copy(data)
    self._Save()

  def Data(self, fresh=False):
    """Returns a reference to the local data, reloading it first only if
    fresh is true.
    """
    if fresh:
      self._Load()
    return self._data


class DictValueDataFile(JSONDataFile):
  """A JSON data file designed to store a dict mapping a string to a number."""
  def __init__(self, filename, mode=None, varname=None):
    JSONDataFile.__init__(self, filename, default={}, mode=mode,
                          varname=varname)

  def AdjustValue(self, key, adjustment):
    """Adjusts the number saved for the key by the adjustment amount, which
    must be a number.

    If the key is not found in the dictionary, adds it with the adjustment
    value.

    Returns the ending value for that key.
    """
    self._Load()
    dict = self.GetDict()
    value = dict.get(key, 0) + adjustment
    dict[key] = value
    self._Save()
    return value

  def GetDict(self, fresh=False):
    """Wrapper for Data() to make the type more self-evident."""
    return self.Data(fresh)


class UniqueListDataFile(JSONDataFile):
  """A JSON data file designed to store a list of unique strings (ASCII or
  Unicode).

  Items will be stored in the order in which they were first added or last
  modified.
  """
  # This uses a list instead of a set() because a set isn't serializable by
  # simplejson.
  def __init__(self, filename, default=None, size=None, mode=None,
               varname=None):
    """The size, if given, restricts the list to that many entries."""
    if size is None:
      self._size = None
    else:
      self._size = int(size)
    JSONDataFile.__init__(self, filename, default=[], mode=mode,
                          varname=varname)

  def Append(self, new):
    """Appends new data to the list, if it's not already there."""
    self._Load()
    data = self.GetList()
    if new not in data:
      data.append(new)
      if self._size is not None and len(data) > self._size:
        data.pop(0)
      self._Save()

  def Update(self, old, new):
    """Deletes old_data and adds new_data, but only if old_data is present."""
    self._Load()
    data = self.GetList()
    if old in data:
      data.remove(old)
      if new not in data:
        data.append(new)
      self._Save()

  def GetList(self, fresh=False):
    """Wrapper for Data() to make the type more self-evident."""
    return self.Data(fresh)
