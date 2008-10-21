#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Set of basic operations/utilities that are used by the build. """

import copy
import errno
import fnmatch
import math
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import zipfile
from twisted.spread import banana

# By default, the banana's string size limit is 640kb, which is unsufficient
# when passing diff's around. Raise it to 100megs. Do this here since the limit
# is enforced on both the server and the client so both need to raise the
# limit.
banana.SIZE_LIMIT = 100 * 1024 * 1024


# Local errors.
class MissingArgument(Exception): pass
class PathNotFound(Exception): pass
class ExternalError(Exception): pass

def MeanAndStandardDeviation(data):
  """Calculates mean and standard deviation for the values in the list.

    Args:
      data: list of numbers

    Returns:
      Mean and standard deviation for the numbers in the list.
  """
  n = len(data)
  mean = float(sum(data)) / n
  variance = sum([(element - mean)**2 for element in data]) / n
  return mean, math.sqrt(variance)


def FilteredMeanAndStandardDeviation(data):
  """Calculates mean and standard deviation for the values in the list
  ignoring first occurence of max value.

    Args:
      data: list of numbers

    Returns:
      Mean and standard deviation for the numbers in the list ignoring
      first occurence of max value.
  """
  def _FilterMax(array):
    new_array = copy.copy(array) # making sure we are not creating side-effects
    new_array.remove(max(new_array))
    return new_array
  return MeanAndStandardDeviation(_FilterMax(data))


class InitializePartiallyWithArguments:
  """Function currying implementation.

  Works for constructors too. Primary use is to be able to construct a class
  with some constructor arguments beings set ahead of actual initialization.
  Copy of an ASPN cookbook (#52549).
  """
  def __init__(self, clazz, *args, **kwargs):
    self.clazz = clazz
    self.pending = args[:]
    self.kwargs = kwargs.copy()

  def __call__(self, *args, **kwargs):
    if kwargs and self.kwargs:
      kw = self.kwargs.copy()
      kw.update(kwargs)
    else:
      kw = kwargs or self.kwargs

    return self.clazz(*(self.pending + args), **kw)


def Prepend(filepath, text):
  """ Prepends text to the file.

  Creates the file if it does not exist.
  """
  file_data = text
  if os.path.exists(filepath):
    file_data += open(filepath).read()
  file = open(filepath, 'w')
  file.write(file_data)
  file.close()


def MaybeMakeDirectory(*path):
  """Creates an entire path, if it doesn't already exist."""
  file_path = os.path.join(*path)
  try:
    os.makedirs(file_path)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise


def RemoveFile(*path):
  """Removes the file located at 'path', if it exists."""
  file_path = os.path.join(*path)
  try:
    os.remove(file_path)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


def LocateFiles(pattern, root=os.curdir):
  """Yeilds files matching pattern found in root and its subdirectories.
  
  An exception is thrown if root doesn't exist."""
  for path, dirs, files in os.walk(os.path.abspath(root)):
    for filename in fnmatch.filter(files, pattern):
      yield os.path.join(path, filename)


def RemoveFilesWildcards(file_wildcard, root=os.curdir):
  """Removes files matching 'file_wildcard' in root and its subdirectories, if
  any exists.
  
  An exception is thrown if root doesn't exist."""
  for file in LocateFiles(file_wildcard, root):
    try:
      os.remove(file)
    except OSError, e:
      if e.errno != errno.ENOENT:
        raise


def RemoveDirectory(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  win32 = False
  if sys.platform == 'win32':
    win32 = True
    # Some people don't have the APIs installed. In that case we'll do without.
    try:
      win32api = __import__('win32api')
      win32con = __import__('win32con')
    except ImportError:
      win32 = False

    def remove_with_retry(rmfunc, path):
      os.chmod(path, stat.S_IWRITE)
      if win32:
        win32api.SetFileAttributes(path, win32con.FILE_ATTRIBUTE_NORMAL)
      try:
        return rmfunc(path)
      except EnvironmentError, e:
        if e.errno != errno.EACCES:
          raise
        print 'Failed to delete %s: trying again' % repr(path)
        time.sleep(0.1)
        return rmfunc(path)
  else:
    def remove_with_retry(rmfunc, path):
      if os.path.islink(path):
        return os.remove(path)
      else:
        return rmfunc(path)

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(os.rmdir, os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)

def CopyFileToDir(src_path, dest_dir):
  """Copies the file found at src_path to the same filename within the
  dest_dir directory.

  Raises PathNotFound if either the file or the directory is not found.
  """
  # Verify the file and directory separately so we can tell them apart and
  # raise PathNotFound rather than shutil.copyfile's IOError.
  if not os.path.isfile(src_path):
    raise PathNotFound('Unable to find file %s' % src_path)
  if not os.path.isdir(dest_dir):
    raise PathNotFound('Unable to find dir %s' % dest_dir)
  src_file = os.path.basename(src_path)
  shutil.copyfile(src_path, os.path.join(dest_dir, src_file))


def MakeZip(output_dir, archive_name, file_list, file_relative_dir,
            raise_error=True):
  """Packs files into a new zip archive.

  Files are first copied into a directory within the output_dir named for
  the archive_name, which will be created if necessary and emptied if it
  already exists.  The files are then then packed using archive names
  relative to the output_dir.  That is, if the zipfile is unpacked in place,
  it will create a directory identical to the new archiev_name directory, in
  the output_dir.  The zip file will be named as the archive_name, plus
  '.zip'.

  Args:
    output_dir: Absolute path to the directory in which the archive is to
      be created.
    archive_dir: Subdirectory of output_dir holding files to be added to
      the new zipfile.
    file_list: List of paths to files or subdirectories, relative to the
      file_relative_dir.
    file_relative_dir: Absolute path to the directory containing the files
      and subdirectories in the file_list.
    raise_error: Whether to raise a PathNotFound error if one of the files in
      the list is not found.

  Returns:
    A tuple consisting of (archive_dir, zip_file_path), where archive_dir
    is the full path to the newly created archive_name subdirectory.

  Raises:
    PathNotFound if any of the files in the list is not found, unless
    raise_error is False, in which case the error will be ignored.
  """

  # Collect files into the archive directory.
  archive_dir = os.path.join(output_dir, archive_name)
  RemoveDirectory(archive_dir)
  MaybeMakeDirectory(archive_dir)
  for needed_file in file_list:
    needed_file = needed_file.rstrip()
    # These paths are relative to the file_relative_dir.  We need to copy
    # them over maintaining the relative directories, where applicable.
    src_path = os.path.join(file_relative_dir, needed_file)
    dirname, basename = os.path.split(needed_file)
    try:
      if os.path.isdir(src_path):
        shutil.copytree(src_path, os.path.join(archive_dir, needed_file))
      elif dirname != '' and basename != '':
        dest_dir = os.path.join(archive_dir, dirname)
        MaybeMakeDirectory(dest_dir)
        CopyFileToDir(src_path, dest_dir)
      else:
        CopyFileToDir(src_path, archive_dir)
    except PathNotFound, e:
      if raise_error:
        raise

  # Pack the zip file.
  output_file = '%s.zip' % archive_dir
  RemoveFile(output_file)
  print 'Creating %s' % output_file
  def _Addfiles(to_zip_file, dirname, files_to_add):
    for this_file in files_to_add:
      archive_name = this_file
      this_path = os.path.join(dirname, this_file)
      if os.path.isfile(this_path):
        # Store files named relative to the outer output_dir.
        archive_name = this_path.replace(output_dir + os.sep, '')
        to_zip_file.write(this_path, archive_name)
        print 'Adding %s' % archive_name
  zip_file = zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED)
  try:
    os.path.walk(archive_dir, _Addfiles, zip_file)
  finally:
    zip_file.close()
  return (archive_dir, output_file)

def ExtractZip(file, output_dir):
  """ Extract the zip archive in the output directory.
      Based on http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/252508.
  """
  MaybeMakeDirectory(output_dir)

  zf = zipfile.ZipFile(file)

  # grabs all the directories in the zip structure. this is necessary
  # to create the structure before trying to extract the file to it.
  dirs = set()
  for name in zf.namelist():
    dirs.add(os.path.dirname(name))

  # create the directory structure.
  for item in dirs:
    curdir = os.path.join(output_dir, item)
    MaybeMakeDirectory(curdir)

  # extract files to directory structure
  for name in zf.namelist():
    print 'Extracting %s' % name
    outfile = open(os.path.join(output_dir, name), 'wb')
    outfile.write(zf.read(name))
    outfile.flush()
    outfile.close()


def WindowsPath(path):
  """Returns a Windows mixed-style absolute path, given a Cygwin absolute path.

  The version of Python in the Chromium tree uses posixpath for os.path even
  on Windows, so we convert to a mixed Windows path (that is, a Windows path
  that uses forward slashes instead of backslashes) manually.
  """
  # TODO(pamg): make this work for other drives too.
  if path.startswith('/cygdrive/c/'):
    return path.replace('/cygdrive/c/', 'C:/')
  return path


def FindUpwardParent(start_dir, *desired_list):
  """Finds the desired object's parent, searching upward from the start_dir.

  Searches within start_dir and within all its parents looking for the desired
  directory or file, which may be given in one or more path components. Returns
  the first directory in which the top desired path component was found, or
  raises PathNotFound if it wasn't.
  """
  desired_path = os.path.join(*desired_list)
  last_dir = ''
  cur_dir = start_dir
  found_path = os.path.join(cur_dir, desired_path)
  while not os.path.exists(found_path):
    last_dir = cur_dir
    cur_dir = os.path.dirname(cur_dir)
    if last_dir == cur_dir:
      raise PathNotFound('Unable to find %s above %s' %
                         (desired_path, start_dir))
    found_path = os.path.join(cur_dir, desired_path)
  # Strip the entire original desired path from the end of the one found
  # and remove a trailing path separator, if present.
  found_path = found_path[:len(found_path) - len(desired_path)]
  if found_path.endswith(os.sep):
    found_path = found_path[:len(found_path) - 1]
  return found_path


def FindUpward(start_dir, *desired_list):
  """Returns a path to the desired directory or file, searching upward.

  Searches within start_dir and within all its parents looking for the desired
  directory or file, which may be given in one or more path components. Returns
  the full path to the desired object, or raises PathNotFound if it wasn't
  found.
  """
  parent = FindUpwardParent(start_dir, *desired_list)
  return os.path.join(parent, *desired_list)


def RunCommand(command):
  """Runs the command list, printing its output and returning its exit status.

  Prints the given command (which should be a list of one or more strings),
  then runs it and prints its stdout and stderr together to stdout,
  line-buffered, converting line endings to CRLF (see note below).  Waits for
  the command to terminate and returns its status.
  """
  print '\n' + subprocess.list2cmdline(command) + '\n',
  proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, bufsize=1)
  last_flush_time = time.time()
  while proc.poll() == None:
    # Note that Windows Python converts \n to \r\n automatically whenever it
    # encounters it written to a text file (including stdout).  The only way
    # around it is to write to a binary file, which isn't feasible for stdout.
    # So we're stuck with \r\n here even though we explicitly write \n.  (We
    # could write \r instead, which doesn't get converted to \r\n, but that's
    # probably more troublesome for people trying to read the files.)
    line = proc.stdout.readline()
    if line:
      # The comma at the end tells python to not add \n, which is \r\n on
      # Windows.
      print line.rstrip() + '\n',
      # Python on windows writes the buffer only when it reaches 4k. This is
      # not fast enough. Flush each 10 seconds instead.
      if time.time() - last_flush_time >= 10:
        sys.stdout.flush()
        last_flush_time = time.time()
  sys.stdout.flush()
  
  # Write the remaining buffer.
  for line in proc.stdout.readline(): 
    print line.rstrip() + '\n',

  return proc.returncode


def GetCommandOutput(command):
  """Runs the command list, returning its output.

  Prints the given command (which should be a list of one or more strings),
  then runs it and returns its output (stdout and stderr) as a string.

  If the command exits with an error, raises ExternalError.
  """
  proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, bufsize=1)
  output = proc.communicate()[0]
  result = proc.returncode
  if result:
    raise ExternalError('%s: %s' % (subprocess.list2cmdline(command), output))
  return output

def GetGClientCommand():
  """Returns the executable command name, depending on the platform.
  """
  if sys.platform == 'win32':
    # Windows doesn't want to depend on bash.
    return 'gclient.bat'
  else:
    return 'gclient'
