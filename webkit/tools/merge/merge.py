#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does a webkit merge to tip of tree WebKit or a revision
specified on the command line. Looks at trunk/src/WEBKIT_MERGE_REVISION
to find the repository and revision to merge from.

Example usage:
  merge.py
  merge.py --new_revision 12345
  merge.py --diff3-tool kdiff3

"""

import optparse
import os
import re
import subprocess
import xml.dom.minidom

import google.path_utils

class Merger(object):
  """ Does svn merges. """

  DIFF3_WRAPPER_PATH = os.path.join('..', '..', 'webkit', 'tools', 'merge',
      'diff3-wrapper.bat')

  def __init__(self, repository, webkit_root, old_revision,
      new_revision, is_dry_run, svn_path='svn', diff3_tool=None):
    """
    Args:
      repository: the repository that we are merging to/from.
      webkit_root: path to third_party/WebKit directory in which to run svn
          commands.
      old_revision: the revision we are currently merged to.
      new_revision: the revision we are merging to.
      is_dry_run: whether to actually make changes or just print intended
          changes.
      svn_path: the path to the svn executable (or find it in the user's path)
      diff3_tool: the tool for doing 3-way diffs.
    """
    self._repository = repository
    self._webkit_root = webkit_root
    self._old_revision = old_revision
    self._new_revision = new_revision
    self._is_dry_run = is_dry_run
    self._diff3_tool = diff3_tool
    self._svn = svn_path

  def MergeDirectory(self, directory):
    """ Merges the given directory in the repository into the same directory
    in the working copy.
    """
    command = [self._svn, "merge", "--accept", "edit", "-r",
        "%s:%s" % (self._old_revision, self._new_revision),
        "%s/%s" % (self._repository, directory), directory]
    if self._diff3_tool is not None:
      command.append("--diff3-cmd")
      command.append(self.DIFF3_WRAPPER_PATH)
      command.append("-x")
      command.append("--use-%s" % self._diff3_tool)
    print ' '.join(command)
    if not self._is_dry_run:
      returncode = subprocess.call(command, cwd=self._webkit_root, shell=True)
      if returncode != 0:
        raise "Are you sure you're running SVN 1.5?  svn --version to check"

def GetCurrentRepositoryAndRevision(webkit_merge_revision_path):
  """ Gets the repository and revision we're currently merged to according to
  the WEBKIT_MERGE_REVISION file checked in.

  Args:
    webkit_merge_revision_path: path to WEBKIT_MERGE_REVISION file.
  """
  contents = open(webkit_merge_revision_path).read().strip()
  split_contents = contents.split("@")
  return {'repository': split_contents[0], 'old_revision': split_contents[1]}

def GetTipOfTreeRevision(svn, repository):
  """ Gets the tip-of-tree revision for the repository.
  """
  info = subprocess.Popen([svn, "info", "--xml", repository],
      shell=True,
      stdout=subprocess.PIPE).communicate()[0]
  dom = xml.dom.minidom.parseString(info)
  return dom.getElementsByTagName('entry')[0].getAttribute('revision')

def UpdateWebKitMergeRevision(webkit_merge_revision_path, repository,
    new_revision, is_dry_run):
  """ Updates the checked in WEBKIT_MERGE_REVISION file with the repository
  and revision we just merged to.

  Args:
    webkit_merge_revision_path: path to WEBKIT_MERGE_REVISION file.
    repository: repository we've merged to.
    new_revision: revision we've merged to.
    is_dry_run: whether to update the file or just print out the update that
        would be done.
  """
  new_merge_revision = "%s@%s" % (repository, new_revision)
  if is_dry_run:
    print "%s=%s" % (webkit_merge_revision_path, new_merge_revision)
  else:
    print "Updating %s" % webkit_merge_revision_path
    #TODO(ojan): Check that the write suceeded.
    open(webkit_merge_revision_path, "w").write(new_merge_revision)

def UpdateDeps(deps_path, new_revision, is_dry_run):
  contents = open(deps_path).read()
  path = '"webkit_revision": "'
  pattern = re.compile(path + "\d+")
  contents = pattern.sub(path + str(new_revision), contents)
  if is_dry_run:
    print "%s=%s" % (deps_path, contents)
  else:
    print "Updating %s" % deps_path
    open(deps_path, "w").write(contents)

def Sync(is_dry_run):
  sync_command = ["gclient", "sync"]
  if is_dry_run:
    print ' '.join(sync_command)
  else:
    returncode = subprocess.call(sync_command, shell=True)
    if returncode != 0:
      raise "gclient sync failed, go beat darin with a squishy bat."

def main(options, args):
  """ Does the merge and updates WEBKIT_MERGE_REVISION.

  Args:
    options: a dictionary of commandline arguments.
    args: currently unused.
  """
  Sync(options.dry_run)

  webkit_merge_revision_path = google.path_utils.FindUpward(
      google.path_utils.ScriptDir(), 'WEBKIT_MERGE_REVISION')

  repository_and_revision = GetCurrentRepositoryAndRevision(
      webkit_merge_revision_path)
  repository = repository_and_revision['repository']
  old_revision = repository_and_revision['old_revision']

  if options.new_revision:
    new_revision = options.new_revision
  else:
    new_revision = GetTipOfTreeRevision(options.svn, repository)

  webkit_root = google.path_utils.FindUpward(google.path_utils.ScriptDir(),
      'third_party', 'WebKit')

  merger = Merger(repository, webkit_root, old_revision, new_revision,
      options.dry_run, options.svn, options.diff3_tool)
  merger.MergeDirectory("WebCore")
  merger.MergeDirectory("JavaScriptCore")

  UpdateWebKitMergeRevision(webkit_merge_revision_path, repository,
      new_revision, options.dry_run)
  deps_path = google.path_utils.FindUpward(
      google.path_utils.ScriptDir(), 'src', 'DEPS')
  UpdateDeps(deps_path, new_revision, options.dry_run)
  Sync(options.dry_run)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option("", "--diff3-tool", default=None,
      help="Optional. diff3 tool to use. Can be 'kdiff3' or 'beyondcompare'")
  option_parser.add_option("", "--new-revision", default=None,
                           help="Optional. Revision to merge to. Tip of tree "
                                "revision will be used if omitted.")
  option_parser.add_option("", "--dry-run", action="store_true", default=False,
                           help="Print out actions the merge would take, but "
                                "don't actually do them.")
  option_parser.add_option("", "--svn", default="svn",
                           help="Path to svn executable, if it's not in your "
                                "PATH.")
  options, args = option_parser.parse_args()
  main(options, args)
