#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to archive a build and its symbols, executed by a buildbot slave.

  This script is used for both developer and official builds.  The archive
  locations are controlled by the --mode option: '--mode official' for
  official builds, '--mode full' for builds to be downlaoded by tester
  buildbots, or '--mode dev' (the default) otherwise.

  Apart from archive locations, the dev and official modes are nearly
  identical, with the exception that the developer build doesn't save the raw
  .exe and .dll files, only the zipped archive. Also official mode copies some
  extra files that are specified in list OFFICIAL_EXTRA_ARCHIVE_FILES.  The
  full mode, on the other hand, saves all the test executables as well.

  When this is run, the current directory (cwd) should be the outer build
  directory (e.g., chrome-release/build/).

  For a list of command-line options, call this script with '--help'.
"""

import glob
import optparse
import os
import re
import shutil
import sys

import chromium_config as config
import chromium_utils
import img_fingerprint as img
import pdb_fingerprint_from_img as pdb
import slave_utils
import source_index as pdb_index


# Set DRY_RUN to True to avoid making changes to external symbol and build
# directories, for testing.
DRY_RUN = False


# The official build copies these .pdb files, plus all the .exe and .dll files
# listed in the FILES file, to its www_dir (see below).  It then uploads those
# .exe and .dll files, except any listed below to be skipped, to the symbol
# server. The dev build archives these .pdb files into a .zip file and copies
# that into its www_dir.
SYMBOLS_COPY = config.Archive.symbols_to_archive

# These .exe and .dll files from the FILES list will be copied to the www_dir
# but not uploaded to the symbol server (generally because they have no symbols
# and thus would produce an error).
SYMBOLS_UPLOAD_SKIP = config.Archive.symbols_to_skip_upload

# These files are archived only for official mode
OFFICIAL_EXTRA_ARCHIVE_FILES = config.Archive.official_extras

# Independent of any other configuration, these exes and any symbol files
# derived from them (i.e., any filename starting with these strings) will not
# be archived or uploaded, typically because they're not built for the current
# distributon.
IGNORE_EXES = config.Archive.exes_to_skip_entirely

INSTALLER_EXE = config.Archive.installer_exe

# This is passed to breakpad/symupload.exe.
SYMBOL_URL = config.Archive.symbol_url
SYMBOL_STAGING_URL = config.Archive.symbol_staging_url

DEV_BASE_DIRS = {
    # Built files are stored here, in a subdir. named for the build version.
    'www_dir_base': config.Archive.www_dir_base_dev,

    # Symbols are stored here, in a subdirectory named for the build version.
    'symbol_dir_base': config.Archive.symbol_dir_base_dev
    }

OFFICIAL_BASE_DIRS = {
    'www_dir_base': config.Archive.www_dir_base_official,
    'symbol_dir_base': config.Archive.symbol_dir_base_official
    }

FULL_BASE_DIRS = {
    'www_dir_base': config.Archive.www_dir_base_full,
    'symbol_dir_base': config.Archive.symbol_dir_base_full
    }


class StagingError(Exception): pass


class Stager(object):
  """Handles staging a build. Call the public StageBuild() method."""

  def __init__(self):
    """Sets a number of file and directory paths for convenient use."""
    self._chrome_dir = os.path.abspath(options.build_dir)
    self._build_dir = os.path.join(self._chrome_dir, options.target)
    self._tool_dir = os.path.join(self._chrome_dir, 'tools', 'build', 'win')
    self._staging_dir = slave_utils.GetStagingDir(self._chrome_dir)

    self._symbol_dir_base = options.dirs['symbol_dir_base']
    self._www_dir_base = options.dirs['www_dir_base']
    if not options.official:
      build_name = slave_utils.SlaveBuildName(self._chrome_dir)
      self._symbol_dir_base = os.path.join(self._symbol_dir_base, build_name)
      self._www_dir_base = os.path.join(self._www_dir_base, build_name)

    self._version_file = os.path.join(self._chrome_dir, 'VERSION')
    self._installer_file = os.path.join(self._build_dir, INSTALLER_EXE)
    self._last_change_file = os.path.join(self._staging_dir, 'LAST_CHANGE')

  def _LastBuildRevision(self):
    """Reads the last staged build revision from _last_change_file.

    If the _last_change_file does not exist, returns -1.
    """
    if os.path.exists(self._last_change_file):
      return int(open(self._last_change_file).read())
    return -1

  def _BuildVersion(self):
    """Returns the full build version string constructed from information in
    _version_file.  Any segment not found in that file will default to '0'.
    """
    major = 0
    minor = 0
    build = 0
    patch = 0
    for line in open(self._version_file, 'r'):
      line = line.rstrip()
      if line.startswith('MAJOR='):
        major = line[6:]
      elif line.startswith('MINOR='):
        minor = line[6:]
      elif line.startswith('BUILD='):
        build = line[6:]
      elif line.startswith('PATCH='):
        patch = line[6:]
    return '%s.%s.%s.%s' % (major, minor, build, patch)

  def _VerifyFiles(self):
    """Ensures that the needed directories and files are accessible.

    Returns a list of any that are not available.
    """
    not_found = []
    needed = open(os.path.join(self._tool_dir, 'FILES')).readlines()
    needed = [os.path.join(self._build_dir, x.rstrip()) for x in needed]
    needed.extend([self._version_file, self._installer_file])
    needed = self._RemoveIgnored(needed)
    for needed_file in needed:
      if not os.path.exists(needed_file):
        not_found.append(needed_file)
    return not_found

  def _GetDifferentialInstallerFile(self, version):
    """Returns the file path for incremental installer if present.

    Checks for file name of format *_<current version>_mini_installer.exe and
    returns the full path of first such file found.
    """
    for file in glob.glob(os.path.join(self._build_dir,
                                       '*' + version + '_' + INSTALLER_EXE)):
      return file
    return None

  def _RemoveIgnored(self, file_list):
    """Returns a list of the file paths in file_list whose filenames don't
    start with any of the strings in IGNORE_EXEs.

    file_list may contain bare filenames or paths. For paths, only the base
    filename will be compared to to IGNORE_EXEs.
    """
    def _IgnoreFile(filename):
      """Returns True if the filename starts with any of the strings in
      IGNORE_EXEs.
      """
      for ignore in IGNORE_EXES:
        if filename.startswith(ignore):
          return True
      return False
    return [x for x in file_list if not _IgnoreFile(os.path.basename(x))]

  def StageBuild(self):
    """Zips build files and uploads them, their symbols, and a change log."""
    result = 0
    print 'Staging in %s' % self._staging_dir

    # Check files and revision numbers.
    not_found = self._VerifyFiles()
    current_revision = slave_utils.SubversionRevision(self._chrome_dir)
    print 'last change: %d' % current_revision
    previous_revision = self._LastBuildRevision()
    if options.official:
      build_version = self._BuildVersion()
      print 'version: %s' % build_version
    else:
      build_version = str(current_revision)
    if current_revision <= previous_revision:
      # If there have been no changes, report it but don't raise an exception.
      # Someone might have pushed the "force build" button.
      print 'No changes since last build (r%d <= r%d)' % (current_revision,
                                                          previous_revision)
      return 0

    # Create the zip archive.
    zip_file_list = open(os.path.join(self._tool_dir, 'FILES')).readlines()
    (zip_dir, zip_file) = chromium_utils.MakeZip(self._staging_dir,
                                               'chrome-win32',
                                               zip_file_list,
                                               self._build_dir,
                                               raise_error=options.official)
    if not os.path.exists(zip_file):
      raise StagingError('Failed to make zip package %s' % zip_file)

    # Generate a change log, or an error message if no previous revision.
    if previous_revision == -1:
      changelog = 'Unknown previous revision: no change log produced.'
    else:
      first_revision = previous_revision + 1
      command = [slave_utils.SubversionExe(), 'log', self._chrome_dir, '--xml',
                 '-r', '%s:%s' % (first_revision, current_revision)]
      changelog = chromium_utils.GetCommandOutput(command)
    changelog_path = os.path.join(self._staging_dir, 'changelog.xml')
    f = open(changelog_path, 'w')
    try:
      f.write(changelog)
    finally:
      f.close()

    # Upload symbols to a share.  Also upload some directly to the crash
    # server, because the crash server does not know how to deal with crashes
    # that happen when winserver has yet to process the newly uploaded symbols.
    symbols_upload = []
    for filename in set(os.listdir(zip_dir)) - set(SYMBOLS_UPLOAD_SKIP):
      if filename.endswith('.dll') or filename.endswith('.exe'):
        symbols_upload.append(os.path.join(self._build_dir, filename))
    if options.official:
      symbols_copy = SYMBOLS_COPY
      symbols_copy.extend(SYMBOLS_UPLOAD_SKIP)
      symbols_copy = [os.path.join(self._build_dir, x) for x in symbols_copy]
      symbols_copy.extend(symbols_upload)
      symbols_copy = self._RemoveIgnored(symbols_copy)
    else:
      # Create a zip archive of the symbol files.  This must be done after the
      # main zip archive is created, or the latter will include this one too.
      (sym_zip_dir, sym_zip_file) = chromium_utils.MakeZip(self._staging_dir,
                                    'chrome-win32-syms',
                                    self._RemoveIgnored(SYMBOLS_COPY),
                                    self._build_dir,
                                    raise_error=False)
      if not os.path.exists(sym_zip_file):
        raise StagingError('Failed to make zip package %s' % sym_zip_file)
      symbols_copy = [sym_zip_file]  # An absolute path.

    # symbols_copy and symbols_upload should hold absolute paths at this point.
    # We avoid joining absolute paths because the version of python used by
    # the buildbots doesn't have the correct Windows os.path.join(), so it
    # doesn't understand C:/ and incorrectly concatenates the absolute paths.
    symbol_dir = os.path.join(self._symbol_dir_base, build_version)
    print 'os.makedirs(%s)' % symbol_dir
    if not DRY_RUN:
      chromium_utils.MaybeMakeDirectory(symbol_dir)
    for filename in symbols_copy:
      print 'chromium_utils.CopyFileToDir(%s, %s)' % (filename, symbol_dir)
      if not DRY_RUN:
        chromium_utils.CopyFileToDir(filename, symbol_dir)

    # Create the symbols used by the source server.
    if options.official:
      symsrc_path = os.path.join(self._build_dir, 'symsrc')

      # Recreate the directory if it exists.
      print 'chromium_utils.RemoveDirectory(%s)' % symsrc_path
      print 'chromium_utils.MaybeMakeDirectory(%s)' % symsrc_path
      if not DRY_RUN:
        chromium_utils.RemoveDirectory(symsrc_path)
        chromium_utils.MaybeMakeDirectory(symsrc_path)

      for filename in config.Archive.symsrc_binaries:
        img_id = img.GetImgFingerprint(os.path.join(self._build_dir, filename))
        (pdb_id, pdb_path) = pdb.GetPDBInfoFromImg(
                                 os.path.join(self._build_dir, filename))

        pdb_name = os.path.basename(pdb_path)
        print "Extracting source for %s:%s %s:%s" % (filename, img_id,
                                                     pdb_name, pdb_id)

        # Create the directory structure for the symbols and images.
        img_dest_path = os.path.join(symsrc_path, filename, str(img_id))
        pdb_dest_path = os.path.join(symsrc_path, pdb_name, str(pdb_id))
        img_path = os.path.join(self._build_dir, filename)

        print 'chromium_utils.MaybeMakeDirectory(%s)' % img_dest_path
        print 'chromium_utils.MaybeMakeDirectory(%s)' % pdb_dest_path
        print 'chromium_utils.CopyFileToDir(%s, %s)' % (img_path, img_dest_path)
        print 'chromium_utils.CopyFileToDir(%s, %s)' % (pdb_path, pdb_dest_path)
        print 'pdb_index.UpdatePDB(%s)' % os.path.join(pdb_dest_path, pdb_name)
        if not DRY_RUN:
          chromium_utils.MaybeMakeDirectory(img_dest_path)
          chromium_utils.MaybeMakeDirectory(pdb_dest_path)

          # Copy the pdb and image files to the directories.
          chromium_utils.CopyFileToDir(img_path, img_dest_path)
          chromium_utils.CopyFileToDir(pdb_path, pdb_dest_path)

          # Update the pdb with the source information.
          pdb_index.UpdatePDB(os.path.join(pdb_dest_path, pdb_name))   

    # Upload build.
    www_dir = os.path.join(self._www_dir_base, build_version)
    installer_destination_file = os.path.join(www_dir, INSTALLER_EXE)
    incremental_installer = self._GetDifferentialInstallerFile(build_version)
    print 'os.makedirs(%s)' % www_dir
    print ('shutil.copyfile(%s, %s)' %
        (self._installer_file, installer_destination_file))
    if incremental_installer:
      print ('chromium_utils.CopyFileToDir(%s, %s)' % (incremental_installer,
                                                      www_dir))
    print 'chromium_utils.CopyFileToDir(%s, %s)' % (zip_file, www_dir)
    print 'chromium_utils.CopyFileToDir(%s, %s)' % (changelog_path, www_dir)
    if options.official:
      extras_with_path  = [os.path.join(self._build_dir, *x)
                           for x in OFFICIAL_EXTRA_ARCHIVE_FILES]
    else:
      extras_with_path = []
    for filename in extras_with_path:
      print 'chromium_utils.CopyFileToDir(%s, %s)' % (filename, www_dir)

    source_copy = os.path.join(self._build_dir, 'symsrc')
    dest_copy =  os.path.join(www_dir, 'symsrc')  
    if options.official:
      print 'shutil.copytree(%s, %s)' % (source_copy, dest_copy)

    if not DRY_RUN:
      chromium_utils.MaybeMakeDirectory(www_dir)
      shutil.copyfile(self._installer_file, installer_destination_file)
      if incremental_installer:
        chromium_utils.CopyFileToDir(incremental_installer, www_dir)
      chromium_utils.CopyFileToDir(zip_file, www_dir)
      chromium_utils.CopyFileToDir(changelog_path, www_dir)
      for filename in extras_with_path:
        chromium_utils.CopyFileToDir(filename, www_dir)

      # Archives the symbols for the source server.
      if options.official:
        shutil.copytree(source_copy, dest_copy)

    # Upload some test executables for distributed and QA-driven tests.
    test_files = config.Archive.tests_to_archive
    test_files = [os.path.join(self._build_dir, x) for x in test_files]

    for glob_dir in config.Archive.test_dirs_to_archive:
      test_files += glob.glob(os.path.join(self._build_dir, glob_dir, '*'))
    test_dirs = config.Archive.test_dirs_to_create

    test_dirs = [os.path.join(www_dir, 'chrome-win32.test', d)
                 for d in test_dirs]
    base_src_dir = os.path.join(self._build_dir, '')

    for test_dir in test_dirs:
      print 'chromium_utils.MaybeMakeDirectory(%s)' % test_dir
    for test_file in test_files:
      relative_dir = os.path.dirname(test_file[len(base_src_dir):])
      test_dir = os.path.join(www_dir, 'chrome-win32.test', relative_dir)
      print 'chromium_utils.CopyFileToDir(%s, %s)' % (test_file, test_dir)
    if not DRY_RUN:
      for test_dir in test_dirs:
        chromium_utils.MaybeMakeDirectory(test_dir)
      for test_file in test_files:
        relative_dir = os.path.dirname(test_file[len(base_src_dir):])
        test_dir = os.path.join(www_dir, 'chrome-win32.test', relative_dir)
        chromium_utils.CopyFileToDir(test_file, test_dir)

    if not DRY_RUN:
      if not options.official:
        # Record the latest revision in the developer archive directory.  QEMU
        # uses this to know when it needs to run another test.
        latest_file_path = os.path.join(self._www_dir_base, 'LATEST')
        print 'Saving revision to %s' % latest_file_path
        latest_file = open(latest_file_path, 'w')
        try:
          latest_file.write('%s' % current_revision)
        finally:
          latest_file.close()

      # Save the current build revision locally so we can compute a changelog
      # next time.
      print 'Saving revision to %s' % self._last_change_file
      change_file = open(self._last_change_file, 'w')
      try:
        change_file.write('%s' % current_revision)
      finally:
        change_file.close()

    if len(not_found):
      sys.stderr.write('\n\nWARNING: File(s) not found: %s\n' %
                       ', '.join(not_found))
    return result

  def FullBuild(self):
    """Zips build directory."""
    print 'Full Staging in %s' % self._build_dir

    # Create the zip archive. Ignore all .ilk/.pdb files and the obj and
    # lib directories.
    zip_file_list = [file for file in os.listdir(self._build_dir)
                     if not re.match('^.+\.((ilk)|(pdb))$', file)]
    zip_file_list.remove('obj')
    zip_file_list.remove('lib')

    (zip_dir, zip_file) = chromium_utils.MakeZip(self._staging_dir,
                                                 'full-build-win32',
                                                 zip_file_list,
                                                 self._build_dir,
                                                 raise_error=True)
    chromium_utils.RemoveDirectory(zip_dir)
    if not os.path.exists(zip_file):
      raise StagingError('Failed to make zip package %s' % zip_file)

    return 0

def main(options, args):
  if options.mode == 'official':
    options.dirs = OFFICIAL_BASE_DIRS
    options.official = True
  elif options.mode == 'dev':
    options.dirs = DEV_BASE_DIRS
    options.official = False
  elif options.mode == 'full':
    options.dirs = FULL_BASE_DIRS
    options.official = False
  else:
    raise StagingError('Invalid options mode %s' % options.mode)

  s = Stager()
  if options.mode == 'full':
    return s.FullBuild()
  return s.StageBuild()

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--mode', default='dev',
      help='switch indicating how to archive build (dev, full, or official)')
  option_parser.add_option('', '--target', default='Release',
      help='build target to archive (Debug or Release)')
  option_parser.add_option('', '--build-dir', default='chrome',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')

  options, args = option_parser.parse_args()
  sys.exit(main(options, args))
