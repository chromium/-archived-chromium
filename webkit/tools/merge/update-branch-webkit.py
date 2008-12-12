#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Update a copy of WebKit to a given version, issuing the necessary gvn commands.


This script is intended for internal use only, by knowledgeable users, and
therefore does not taint-check user-supplied values.

Cygwin is required to run this script, for 'cygpath'.

Only tested on Windows, running in Cygwin.  Most of this script should
be compatible with Linux as well.  The paths may not be correct for
running in a Windows cmd shell.

TODO(pamg): Test in Windows cmd shell and fix problems.

Usage notes:
  In normal usage, it's expected that the user will first run svn manually
  to update a local copy of the WebKit trunk to a known (or head) revision.
  The svn checkout is prone to odd failures (network timeouts, etc.), so
  it's safer to run it by hand so you can more easily notice and recover
  from any problems.  Note that if you're running svn more than once, to
  update more than one directory, you'll want to specify a --revision (-r)
  on any calls after the first, just in case someone's landed something
  while you were checking out the first directory.
  For example:

  cd /tmp/webkit-branch-merge/WebKit
  svn co http://svn.webkit.org/repository/webkit/trunk/WebCore WebCore
    [...Checked out revision 19776.]
  svn co -r 19776 \
      http://svn.webkit.org/repository/webkit/trunk/JavaScriptCore \
      JavaScriptCore
  cd /path/to/webkit/tools/merge
  python ./update-branch-webkit.py --diff --real \
      --old /path/to/mergebranch/ 19776

  Here the --old parameter points to the root of your merge branch checkout,
  containing the src/ and data/ directories.  You can also append
  --new [directory name] to point to the directory where you've checked out
  the latest webkit files (if you checked them out to somewhere other than
  /tmp/webkit-branch-merge/WebKit as depicted above).  The --new parameter
  should be a "WebKit" directory that corresponds to the webkit svn trunk.

  If you want to see what the script will do before doing it, remove the
  --real option.  When you're satisfied that it's doing the right thing, you
  can restore --real and run it one more time.  In that case, you can also
  leave off --diff on each run after the first, allowing the script to use a
  cached file of diffs rather than re-generating it each time.  Other options
  are available as well; see 'update-branch-webkit --help' for a full list.

  Directories may be specified with Windows-style paths, but the slashes
  must be forward-slashes, not backslashes, or the Cygwin shell will remove
  them.
"""

import errno
import logging
import optparse
import os
import re
import shutil
import subprocess
import sys
import traceback

# Whether to produce additional debugging information.
DEBUGGING = False

# The filename holding the new WebKit Subversion revision number.
BASE_REV_FILE = 'BASE_REVISION'

# The URL from which to checkout the new revision if needed.
SVN_URL = 'http://svn.webkit.org/repository/webkit/trunk'

# The files in which to save lists of added, deleted, and edited files.
SAVE_ADDED_FILES    = 'merge-add-files.txt'
SAVE_DELETED_FILES  = 'merge-delete-files.txt'
SAVE_EDITED_FILES   = 'merge-edit-files.txt'
SAVE_OBSOLETE_FILES = 'merge-obsolete-files.txt'

# Executable names, found in the system PATH.
SVN = 'svn.exe'
DIFF = 'diff.exe'
CYGPATH = 'cygpath.exe'

# Directories to be entirely ignored when updating the merge branch.
IGNORE_DIRS = ['.svn', 'DerivedSources']

# Global cygpath process.
_cygpath_proc = None

########################################
# Error classes

class LocalError(Exception):
  """Base class for local errors."""
  stack_trace = True
  code = 1

class ArgumentError(LocalError):
  """Exception raised for errors in parsed script arguments."""
  stack_trace = False
  code = 2

class CannotFindError(LocalError):
  """Exception raised for an expected file or directory that does not exist."""
  def __init__(self, description, path):
    LocalError.__init__(self, "Can't find %s '%s'" % (description, path))
  stack_trace = False
  code = 3

class CannotWriteError(LocalError):
  """Exception raised when writing to a necessary file fails."""
  def __init__(self, path):
    LocalError.__init__(self, "Can't write to '%s'" % path)
  stack_trace = False
  code = 4

class CommandError(LocalError):
  """Exception raised when an external command fails."""
  def __init__(self, prefix, value):
    """
    Build a descriptive error message using available information.

    Args:
      prefix: Descriptive text prepended to the error message.
      value: Error value as provided in the exception, if any.  May be None.
    """
    if value is not None:
      (errnum, message) = value
      LocalError.__init__(self, "%s: %s (%s)" % (prefix, message, errnum))
    else:
      LocalError.__init__(self, prefix)
  stack_trace = False
  code = 5


########################################
# Utility functions

def GetAbsolutePath(path):
  """Convert an unknown-style path to an absolute, mixed-style Windows path.

  We use an external cygpath binary to do the conversion.  For performance
  reasons, we use a single cygpath process.
  """
  global _cygpath_proc
  if not _cygpath_proc:
    _cygpath_proc = subprocess.Popen([CYGPATH, '-a', '-m', '-f', '-'],
                                     stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE)
  _cygpath_proc.stdin.write(path + '\n')
  return _cygpath_proc.stdout.readline().rstrip()

def PathFromChromeRoot(subdir):
  """Return the path of the given WebKit path relative to the Chrome merge dir.

  We could get fancy and search for these, but it's so much simpler to just
  set them, it's arguably even easier to maintain that way.

  Args:
    subdir: The relative path to be converted.  It may be a directory or a
        filename, and it should be specified relative to the WebKit checkout
        (e.g., 'WebCore').

  Returns:
    The path to the given subdirectory dir in the Chrome webkit tree, starting
    from the src directory.
  """
  return os.path.join('third_party', subdir)

def NewDir(subdir=''):
  """Return an absolute path to the subdirectory within the WebKit checkout.

  The subdir is specified relative to the WebKit checkout (e.g., 'WebCore').
  """
  return GetAbsolutePath(os.path.join(options.new, subdir))

def OldDir(subdir=''):
  """Return an absolute path to the given subdirectory within the merge branch.

  The subdir is specified relative to the WebKit checkout (e.g., 'WebCore').
  """
  reldir = PathFromChromeRoot(subdir)
  return GetAbsolutePath(os.path.join(options.old, reldir))

def TempDir(relative_path=''):
  """Return an absolute path to the given location in the temp directory."""
  return GetAbsolutePath(os.path.join(options.temp_dir, relative_path))

def CollectFileList(search_dir, old):
  """Recursively collect a set of files within the given directory.

  FIXME(pamg): This function assumes that there are no instances of the string
  given by the search_dir argument in either of the source paths (old and
  new). That is, if dir is 'WebCore', the --old and --new options must not
  contain the string 'WebCore'.

  Args:
    search_dir: The directory within which to search.  It should be specified
        relative to the WebKit checkout (e.g., 'WebCore').
    old: Boolean value indicating whether to look for files in the old
        directory (i.e., the Chrome merge branch) or the new one (the WebKit
        checkout).

  Returns:
    A set of file paths relative to (and including) the given dir, with no
    leading slash.
  """
  # FIXME(pamg): This replacement is why --old and --new can't contain
  # search_dir.
  if old:
    path = OldDir(search_dir)
    basedir = path.replace(search_dir, '')
  else:
    path = NewDir(search_dir)
    basedir = path.replace(search_dir, '')

  result = set()
  for root, dirs, files in os.walk(path):
    # We want the directory from search_dir onward, and we need '/' separators
    # to match what diff returns.
    relative_dir = root.replace(basedir, '', 1).replace('\\', '/')
    for filename in files:
      if not IgnoreFile(root):
        result.add('/'.join([relative_dir, filename]))
  return result

def MakeDirectory(path):
  """Create a path and its parents, returning a set of the paths created."""

  logging.debug('Making directory %s' % path)
  add = set()
  (basepath, subdir) = os.path.split(path)
  if not os.path.exists(basepath):
    add = add.union(MakeDirectory(basepath))
  if options.real:
    os.mkdir(path)
  add.add(path)
  return add

def IsAvailable(executable):
  """Return True if the given executable can be found.

  An executable is deemed available if it exists (for an absolute or relative
  path) or is in the system path (for a simple filename).  This is a naive
  implementation that assumes that if a file with the given name exists, it is
  accessible and executable.
  """

  if os.path.dirname(executable) != '':
    # This is a pathname, either relative or absolute.
    return os.path.exists(executable)

  paths = os.environ['PATH'].split(os.pathsep)
  for path_dir in paths:
    if os.path.exists(os.path.join(path_dir, executable)):
      return True
  return False

def IgnoreFile(path):
  """Return True if the path is in the list of directories to be ignored."""
  for ignore_dir in IGNORE_DIRS:
    path = path.replace('\\', '/')
    path = '/' + path + '/'
    if '/' + ignore_dir + '/' in path:
      return True
  return False

def WriteFile(path, data):
  """Write the given data to the path.

  Args:
    path: The absolute filename to be written.
    data: The string data to be written to the file.

  Raises:
    CannotWriteError if the write fails.
  """
  logging.debug('Writing file to %s' % path)
  try:
    f = open(path, 'w')
    f.write(data)
    f.close()
  except IOError, e:
    raise CannotWriteError(path)

def WriteAbsoluteFileList(filename, file_list):
  """Write a file containing a sorted list of absolute paths in the branch.

  Args:
    filename: The file to write.
    file_list: An unsorted list of file paths, given relative to the WebKit
        checkout (e.g., 'WebCore/ChangeLog').
  """
  logging.info('Saving %s' % filename)
  file_paths = [OldDir(x) for x in file_list]
  # Sort after converting to absolute paths, in case some of the incoming paths
  # were already absolute.
  file_paths.sort()
  WriteFile(filename, '\n'.join(file_paths))

def RunShellCommand(command, description, run=True):
  """Run a command in the shell, waiting for completion.

  Args:
    command: The shell command to be run, as a string.
    description: A brief description of the command, to be used in an error
        message if needed.
    run: If false, the command will be logged, but it won't actually be
        executed.

  Raises:
    CommandError if the command runs but exits with a nonzero status.
  """
  logging.debug(command)
  if run:
    result = subprocess.call(command, shell=True)
    if result:
      raise CommandError(description, None)


########################################
# Task functions.

def ValidateArguments():
  """Validate incoming options.

  Canonicalize paths and make sure that they exist, make sure a diff file
  exists if one is being used, and check for all required executables.
  """
  if options.revision is None:
    raise ArgumentError('Please specify a --revision.')
  try:
    # We need the revision as a string, but we want to make sure it's numeric.
    options.revision = str(int(options.revision))
  except ValueError:
    raise ArgumentError('Revision must be a number.')
  if options.revision < 1:
    raise ArgumentError('Revision must be at least 1.')

  options.dir_list = options.directories.split(',')
  if not len(options.dir_list):
    raise ArgumentError('Please list at least one directory.')

  options.old = GetAbsolutePath(options.old)
  options.new = GetAbsolutePath(options.new)
  options.temp_dir = GetAbsolutePath(options.temp_dir)

  if not os.path.exists(options.old):
    raise CannotFindError('old dir', options.old)
  if not os.path.exists(options.new):
    raise CannotFindError('new dir', options.new)
  for dir_name in options.dir_list:
    if not os.path.exists(os.path.join(options.old, 'third_party', dir_name)):
      raise CannotFindError('old %s' % dir_name, '')
    if not os.path.exists(os.path.join(options.new, dir_name)):
      raise CannotFindError('new %s' % dir_name, '')
  if not os.path.exists(options.temp_dir):
    logging.info('Creating temp directory %s' % options.temp_dir)
    os.makedirs(options.temp_dir)

  saved_diff_path = TempDir(SAVE_EDITED_FILES)
  if not options.diff and not os.path.exists(saved_diff_path):
    logging.warning("No saved diff file at '%s'. Running diff again." %
                    saved_diff_path)
    options.diff = True

  if options.svn_checkout:
    if not IsAvailable(SVN):
      raise CannotFindError('executable', SVN)
  else:
    checkout_dir = NewDir()
    if not IsAvailable(checkout_dir):
      raise CannotFindError('new WebKit checkout', checkout_dir)
  if options.diff:
    if not IsAvailable(DIFF):
      raise CannotFindError('executable', DIFF)
  if not IsAvailable('gvn.bat'):
    raise CannotFindError('batch file', 'gvn.bat')

def RunSvnCheckout(revision):
  """Use svn to checkout a new WebKit from SVN_URL into the temp dir."""
  logging.info('Checking out revision %s', revision)
  for checkout_dir in options.dir_list:
    new_dir = NewDir(checkout_dir)
    url = SVN_URL + '/' + checkout_dir
    command = '%s checkout -r %s %s %s' % (SVN, revision, url, new_dir)
    RunShellCommand(command, 'Running svn')

def UpdateBaseRevision(revision):
  """Update the merge branch BASE_REVISION file."""
  new_baserev_file = NewDir(BASE_REV_FILE)
  try:
    existing_revision = open(new_baserev_file, 'r').read().rstrip()
  except IOError, e:
    if e.errno == errno.ENOENT:
      existing_revision = -1
    else:
      raise
  if revision != existing_revision:
    logging.info('Updating %s to %s' % (new_baserev_file, revision))
    WriteFile(new_baserev_file, revision)

def RunDiff(saved_file):
  """Collect a set of files that differ between the WebKit and Chrome trees.

  Run a recursive diff for each of the relevant directories, collecting a set
  of files that differ between them.  Save the resulting list in a file, and
  also return the set.

  Args:
    saved_file: The file in which to save the list of differing files.

  Returns:
    A set of paths to files that differ between the two trees.  Paths are
    given relative to the WebKit checkout (e.g., 'WebCore/ChangeLog').

  Raises:
    CommandError if the diff command returns an error or produces unrecognized
    output.
  """
  # Regular expressions for parsing the output of the diff.
  add_delete_re = re.compile("^Only in [^ ]+")
  edit_re = re.compile("^Files ([^ ]+) and [^ ]+ differ")

  # Generate a set of the possible leading directory strings, to be removed
  # from the file paths reported by diff.
  dir_replacements = set()
  for checkout_dir in options.dir_list:
    # FIXME(pamg): This assumes that the name of the dir being processed is
    # not a substring of options.new.  See the FIXME for CollectFileList.
    base_dir = NewDir(checkout_dir).replace(checkout_dir, '')
    dir_replacements.add(base_dir)

  # Collect the set of edited files.
  edited = set()
  for diff_dir in options.dir_list:
    logging.info('Generating diffs for %s' % diff_dir)
    command = [DIFF, '--recursive', '--ignore-space-change', '--brief',
               NewDir(diff_dir), OldDir(diff_dir)]
    logging.debug(command)
    try:
      p = subprocess.Popen(command, stdout=subprocess.PIPE)
      diff_text = p.stdout.read().replace('\r', '')
      rc = p.wait()
    except OSError, e:
      raise CommandError('Running diff ' + command, e)
    # Diff exit status is 0 if the directories compared are the same, 1 if
    # they differ, or  2 if an error was encountered.
    if rc < 0 or rc > 1:
      raise CommandError('Error running diff command', None)

    # Examine this directory's diff result and build a list of changed files.
    # Although we're not interested in added or deleted files, we parse the
    # "Only in..." lines to catch any errors.
    #   Files C:/new/directory/file.h and C:/old/directory/file.h differ
    #  (ignored) Only in C:/new/directory: new_filename.h
    #  (ignored) Only in C:/old/directory: old_filename.h
    logging.info('Analyzing %s diff results' % diff_dir)
    for diff_line in diff_text.split('\n'):
      if IgnoreFile(diff_line):
        continue
      if edit_re.search(diff_line) is not None:
        filename = re.sub(edit_re, r'\1', diff_line)
        for replace in dir_replacements:
          filename = filename.replace(replace, '', 1)
        edited.add(filename)
      elif add_delete_re.search(diff_line) is not None:
        # Do nothing, but don't complain about an error.
        pass
      elif diff_line != '':
        raise CommandError("Unknown diff result '%s'" % diff_line, None)

  WriteFile(saved_file, '\n'.join(sorted(edited)))
  return edited

def CopyFiles(files):
  """Copy files from WebKit to the merge branch, collecting added directories.

  If options.real is False, collect the directories that would be added, but
  don't actually make the directories or perform the copies.

  Args:
    files: The set of files to be copied, with paths given relative to the
        WebKit checkout (e.g., 'WebCore/ChangeLog').

  Returns:
    A set of absolute directory paths that were created in the merge branch
    in order to copy the files.

  Raises:
    CommandError if the file copy fails.
  """
  logging.info('Copying files from %s to %s' % (options.new, options.old))
  added = set()
  for file in files:
    # Create the directory if it doesn't exist.
    # TODO(pamg): This has a lot of redundancy and could probably be made
    # faster.
    dirname = os.path.dirname(file)
    old_dir = OldDir(dirname)
    if not os.path.exists(old_dir):
      # We need to handle each new directory level individually, for gvn add.
      added = added.union(MakeDirectory(old_dir))
    new_path = NewDir(file)
    old_path = OldDir(file)
    if options.real:
      try:
        shutil.copyfile(new_path, old_path)
      except IOError, e:
        raise CommandError('Copying file', e)
  return added

def CopyObsoleteFiles(files):
  """Copy new versions of obsolete header files into .obsolete names.

  If options.real is False, log the copy but do not actually perform it.

  Args:
    files: The set of header files to be copied, with paths given relative to
        the WebKit checkout and not including '.obsolete' (e.g.,
        'WebCore/html/HTMLElement.h').  Their directories should already exist.

  Raises:
    CommandError if the file copy fails.
  """
  for file in files:
    # The directory should already exist.
    new_path = NewDir(file)
    old_path = OldDir(file + '.obsolete')
    logging.debug('Copying %s to %s' % (new_path, old_path))
    if options.real:
      try:
        shutil.copyfile(new_path, old_path)
      except IOError, e:
        raise CommandError('Copying obsolete file', e)

def CopyBaseRevisionFile():
  """Copy the BASE_REV_FILE from the WebKit checkout to the merge branch.

  If options.real is False, log the copy but do not actually perform it.

  Raises:
    CommandError if the file copy fails.
  """
  logging.info('Copying %s to %s' % (BASE_REV_FILE, OldDir()))
  if options.real:
    try:
      shutil.copyfile(NewDir(BASE_REV_FILE), OldDir(BASE_REV_FILE))
    except IOError, e:
      raise CommandError('Copying %s' % BASE_REV_FILE, e)


def main(options, args):
  """Perform the merge."""

  ValidateArguments()

  # Report configuration.
  logging.info('Updating to revision %s' % options.revision)
  logging.info('Updating into old directory %s' % options.old)
  logging.info('Using WebKit from new directory %s' % options.new)
  logging.info('Using temp directory %s' % options.temp_dir)

  if not options.real:
    logging.warning('DEBUGGING: Not issuing mkdir, cp, or gvn commands')

  if options.svn_checkout:
    RunSvnCheckout(options.revision)

  UpdateBaseRevision(options.revision)

  # Make sure desired directories are in the checkout.
  for checkout_dir in options.dir_list:
    if not os.path.exists(NewDir(checkout_dir)):
      raise CannotFindError('webkit checkout dir', NewDir(checkout_dir))

  # Generate or load diffs between relevant directories in the two trees.
  saved_diff_path = TempDir(SAVE_EDITED_FILES)
  if options.diff:
    edit_files = RunDiff(saved_diff_path)
  else:
    logging.info('Reading diffs from %s' % saved_diff_path)
    diff_text = open(saved_diff_path, 'r').read()
    edit_files = set(diff_text.split('\n'))

  # Collect filenames in each directory.
  old_files = set()
  new_files = set()
  logging.info('Collecting lists of old and new files')
  for collect_dir in options.dir_list:
    logging.debug('Collecting old files in %s' % collect_dir)
    old_files = old_files.union(CollectFileList(collect_dir, old=True))
    logging.debug('Collecting new files in %s' % collect_dir)
    new_files = new_files.union(CollectFileList(collect_dir, old=False))

  add_files = new_files - old_files
  delete_files = old_files - new_files

  if DEBUGGING:
    WriteFile(TempDir('old.txt'), '\n'.join(sorted(old_files)))
    WriteFile(TempDir('new.txt'), '\n'.join(sorted(new_files)))

  # Find any obsolete files ostensibly to be deleted, and rename (rather than
  # re-adding) their corresponding new .h files.  We trust that no filename
  # that ends with '.obsolete' will also contain another '.obsolete' in its
  # name.
  obsolete_files = set()
  # Iterate through a copy of the deleted set since we'll be changing it.
  delete_files_copy = delete_files.copy()
  for deleted in delete_files_copy:
    if deleted.endswith('.obsolete'):
      try:
        # Only the add_files.remove() should be able to raise a KeyError.
        # If we have no original file to rename, don't delete the .obsolete
        # one either.
        delete_files.remove(deleted)
        deleted = deleted.replace('.obsolete', '')
        add_files.remove(deleted)
        obsolete_files.add(deleted)
      except KeyError, e:
        logging.warning('No new file found for old %s.obsolete.' % deleted)

  # Save the list of obsolete files for future reference.
  WriteFile(TempDir(SAVE_OBSOLETE_FILES), '\n'.join(sorted(obsolete_files)))

  # Save the list of deleted files, converted to absolute paths, for use by
  # gvn.
  WriteAbsoluteFileList(TempDir(SAVE_DELETED_FILES), delete_files)

  # Issue gvn deletes. We do this before copying over the added files so that
  # an apparently new file that only differs from an old one in the case of
  # its filename will be copied with the right name even though Windows
  # filenames are case-insensitive.  (Otherwise, if the file cppparser.h
  # already existed in the Chrome branch but was renamed to CPPParser.h in the
  # WebKit checkout, the "added" file CPPParser.h would be copied into
  # cppparser.h rather than using its new name.)
  if len(delete_files):
    logging.info('Issuing gvn delete')
    command = 'gvn --targets %s delete' % TempDir(SAVE_DELETED_FILES)
    RunShellCommand(command, 'Running gvn delete', options.real)

  # Copy added and edited files from the svn checkout, collecting added
  # directories in the process.
  if not options.deletes_only:
    add_dirs = CopyFiles(add_files | edit_files)
    for dir_name in add_dirs:
      # We want relative paths, so truncate the path.
      add_files.add(dir_name[len(OldDir()) + 1:])
    CopyObsoleteFiles(obsolete_files)
    CopyBaseRevisionFile()

  # Save the sorted list of added files, converted to absolute paths, for use
  # by gvn. Sorting ensures that new directories are added before their
  # contents are.
  WriteAbsoluteFileList(TempDir(SAVE_ADDED_FILES), add_files)

  # Issue gvn adds.
  os.chdir(OldDir())
  if len(add_files) and not options.deletes_only:
    logging.info('Issuing gvn add')
    command = 'gvn --targets %s add' % TempDir(SAVE_ADDED_FILES)
    RunShellCommand(command, 'Running gvn add', options.real)

  # Print statistics.
  logging.info('Edited: %s' % len(edit_files))
  logging.info('Added: %s' % len(add_files))
  logging.info('Deleted: %s' % len(delete_files))
  logging.info('Renamed (obsoletes): %s' % len(obsolete_files))

  return 0

if '__main__' == __name__:
  # Set up logging.
  if DEBUGGING:
    loglevel = logging.DEBUG
  else:
    loglevel = logging.INFO
  logging.basicConfig(level=loglevel,
                      format='%(asctime)s %(levelname)-7s: %(message)s',
                      datefmt='%H:%M:%S')

  # We need cygpath to convert our default paths.
  try:
    if not IsAvailable(CYGPATH):
      raise CannotFindError('cygpath', CYGPATH)
  except LocalError, e:
    if e.stack_trace or DEBUGGING:
      traceback.print_exc()
    logging.error(e)
    sys.exit(e.code)

  # Set up option parsing.
  opt = optparse.OptionParser()

  opt.add_option('-r', '--revision', default=None,
      help='(REQUIRED): New WebKit revision (already checked out, or to be)')

  opt.add_option('', '--directories',
                 default='JavaScriptCore,WebCore',
                 help='Comma-separated list of directories to update')

  opt.add_option('', '--old',
                 default=GetAbsolutePath('/cygdrive/c/src/merge/'),
                 help='Path to top of branch checkout, holding src/ and data/')

  opt.add_option('', '--new',
                 default=GetAbsolutePath('/tmp/webkit-branch-merge/WebKit'),
                 help='Directory containing new WebKit checkout')

  opt.add_option('', '--temp-dir',
                 default=GetAbsolutePath('/tmp/webkit-branch-merge'),
                 help='Temp directory available for use (will be created)')

  opt.add_option('', '--svn-checkout', action='store_true',
                 default=False,
                 help='Run a new svn checkout into the new dir')

  opt.add_option('', '--diff', action='store_true',
                 default=False,
                 help='Run a fresh diff rather than using a saved diff file')

  opt.add_option('', '--deletes-only', action='store_true',
                 default=False,
                 help='Only issue gvn commands for deletes (this is useful'
                      'when the case of a filename has changed)')

  opt.add_option('', '--real', action='store_true',
                 default=False,
                 help="I'm ready: do the mkdir, cp, and gvn commands for real")

  (options, args) = opt.parse_args()

  # Run.
  try:
    exit_value = main(options, args)
  except LocalError, e:
    if e.stack_trace or DEBUGGING:
      traceback.print_exc()
    logging.error(e)
    exit_value = e.code

  sys.exit(exit_value)

