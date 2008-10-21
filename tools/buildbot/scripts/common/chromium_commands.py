#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A subclass of commands.SVN that allows more flexible error recovery."""

import os
import re
import sys

from twisted.python import log
from twisted.internet import defer
from buildbot.slave import commands
from buildbot.slave.registry import registerSlaveCommand

import chromium_utils

# Local errors.
class InvalidPath(Exception): pass


class GoogleSVN(commands.SVN):
  """SVN class with more flexible error recovery and no svnversion call.

  In addition to the arguments handled by commands.SVN, this command reads
  the following keys:

  ['rm_timeout']: if not None, a different timeout used only for the 'rm -rf'
                  operation in doClobber.  Otherwise the svn timeout will be
                  used for that operation too.
  """

  def setup(self, args):
    commands.SVN.setup(self, args)
    self.rm_timeout = args.get('rm_timeout', self.timeout)

  def _RemoveDirectoryCommand(self, rm_dir):
    """Returns a command list to delete a directory using Python."""
    # Use / instead of \ in paths to avoid issues with escaping.
    return ['python', '-c',
            'import chromium_utils; '
            'chromium_utils.RemoveDirectory("%s")' % rm_dir.replace('\\', '/')]


  def _RenameDirectoryCommand(self, src_dir, dest_dir):
    """Returns a command list to rename a directory (or file) using Python."""
    # Use / instead of \ in paths to avoid issues with escaping.
    return ['python', '-c',
            'import os; '
            'os.rename("%s", "%s")' %
                (src_dir.replace('\\', '/'), dest_dir.replace('\\', '/'))]


  def doClobber(self, dummy, dirname):
    """Moves the old directory aside, or deletes it if that's already been
    done.

    This function is designed to be used with a source dir.  If it's called
    with anything else, the caller will need to be sure to clean up the
    <dirname>.dead directory once it's no longer needed.

    If this is the first time we're clobbering since we last finished a
    successful update or checkout, move the old directory aside so a human
    can try to recover from it if desired.  Otherwise -- if such a backup
    directory already exists, because this isn't the first retry -- just
    remove the old directory.

    Args:
      dummy: unused
      dirname: the directory within self.builder.basedir to be clobbered
    """
    old_dir = os.path.join(self.builder.basedir, dirname)
    dead_dir = old_dir + '.dead'
    if os.path.isdir(old_dir):
      if os.path.isdir(dead_dir):
        command = self._RemoveDirectoryCommand(dead_dir)
      else:
        command = self._RenameDirectoryCommand(old_dir, dead_dir)
      c = commands.ShellCommand(self.builder, command, self.builder.basedir,
                                sendRC=0, timeout=self.rm_timeout)
      self.command = c
      # See commands.SVN.doClobber for notes about sendRC.
      d = c.start()
      d.addCallback(self._abandonOnFailure)
      return d
    return None

  def writeSourcedata(self, res):
    """Write the sourcedata file and remove any dead source directory."""
    dead_dir = os.path.join(self.builder.basedir, self.srcdir + '.dead')
    if os.path.isdir(dead_dir):
      msg = 'Removing dead source dir'
      self.sendStatus({'header': msg + '\n'})
      log.msg(msg)
      command = self._RemoveDirectoryCommand(dead_dir)
      c = commands.ShellCommand(self.builder, command, self.builder.basedir,
                                sendRC=0, timeout=self.rm_timeout)
      self.command = c
      d = c.start()
      d.addCallback(self._abandonOnFailure)
    open(self.sourcedatafile, 'w').write(self.sourcedata)
    return res

  def parseGotRevision(self):
    """Extract the revision number from the result of the svn operation.

    svn checkout operations finish with 'Checked out revision 16657.'
    svn update operations finish the line 'At revision 16654.'
    """
    SVN_REVISION_RE = re.compile(r'^(Checked out|At) revision ([0-9]+)\.')
    for line in self.command.stdout.splitlines():
      m = SVN_REVISION_RE.search(line)
      if m is not None:
        return int(m.group(2))
    return None

registerSlaveCommand('svn_google', GoogleSVN, commands.command_version)

class GClient(commands.SourceBase):
  """Source class that handles gclient checkouts.

  In addition to the arguments handled by commands.SourceBase, this command
  reads the following keys:

  ['gclient_spec']:
    if not None, then this specifies the text of the .gclient file to use.
    this overrides any 'svnurl' argument that may also be specified.

  ['rm_timeout']:
    if not None, a different timeout used only for the 'rm -rf' operation in
    doClobber.  Otherwise the svn timeout will be used for that operation too.

  ['svnurl']:
    if not None, then this specifies the svn url to pass to 'gclient config'
    to create a .gclient file.

  ['branch']:
    if not None, then this specifies the module name to pass to 'gclient sync'
    in --revision argument

  """

  header = 'gclient'

  def setup(self, args):
    """Our implementation of command.Commands.setup() method.
    The method will get all the arguments that are passed to remote command
    and is invoked before start() method (that will in turn call doVCUpdate()).
    """
    commands.SourceBase.setup(self, args)
    self.vcexe = commands.getCommand('gclient')
    self.svnurl = args['svnurl']
    self.branch =  args.get('branch')
    self.revision = args.get('revision')
    self.patch = args.get('patch')
    self.gclient_spec = args['gclient_spec']
    self.sourcedata = '%s\n' % self.svnurl
    self.rm_timeout = args.get('rm_timeout', self.timeout)

  def start(self):
    """Start the update process.

    start() is cut-and-paste from the base class, the block calling
    self.sourcedirIsPatched() and the revert support is the only functional
    difference from base."""
    self.sendStatus({'header': "starting " + self.header + "\n"})
    self.command = None

    # self.srcdir is where the VC system should put the sources
    if self.mode == "copy":
      self.srcdir = "source" # hardwired directory name, sorry
    else:
      self.srcdir = self.workdir
    self.sourcedatafile = os.path.join(self.builder.basedir,
                                       self.srcdir,
                                       ".buildbot-sourcedata")

    d = defer.succeed(None)
    # do we need to clobber anything?
    if self.mode in ("copy", "clobber", "export"):
      d.addCallback(self.doClobber, self.workdir)
    if not (self.sourcedirIsUpdateable() and self.sourcedataMatches()):
      # the directory cannot be updated, so we have to clobber it.
      # Perhaps the master just changed modes from 'export' to
      # 'update'.
      d.addCallback(self.doClobber, self.srcdir)
    elif self.sourcedirIsPatched():
      # The directory is just patched. Revert the sources.
      d.addCallback(self.doRevert)
      # TODO(maruel): Remove unversioned files.

    d.addCallback(self.doVC)

    if self.mode == "copy":
        d.addCallback(self.doCopy)
    if self.patch:
        d.addCallback(self.doPatch)
    d.addCallbacks(self._sendRC, self._checkAbandoned)
    return d

  def sourcedirIsPatched(self):
    return os.path.exists(os.path.join(self.builder.basedir,
                                       self.srcdir, '.buildbot-patched'))

  def sourcedirIsUpdateable(self):
    # Patched directories are updatable.
    return os.path.exists(os.path.join(self.builder.basedir,
                                       self.srcdir, '.gclient'))

  # TODO(pamg): consolidate these with the copies above.
  def _RemoveDirectoryCommand(self, rm_dir):
    """Returns a command list to delete a directory using Python."""
    # Use / instead of \ in paths to avoid issues with escaping.
    return ['python', '-c',
            'import chromium_utils; '
            'chromium_utils.RemoveDirectory("%s")' % rm_dir.replace('\\', '/')]

  def _RenameDirectoryCommand(self, src_dir, dest_dir):
    """Returns a command list to rename a directory (or file) using Python."""
    # Use / instead of \ in paths to avoid issues with escaping.
    return ['python', '-c',
            'import os; '
            'os.rename("%s", "%s")' %
                (src_dir.replace('\\', '/'), dest_dir.replace('\\', '/'))]

  def _RemoveFileCommand(self, file):
    """Returns a command list to remove a directory (or file) using Python."""
    # Use / instead of \ in paths to avoid issues with escaping.
    return ['python', '-c',
            'import chromium_utils; '
            'chromium_utils.RemoveFile("%s")' % file.replace('\\', '/')]

  def _RemoveFilesWildCardsCommand(self, file_wildcard):
    """Returns a command list to delete files using Python.
    
    Due to shell mangling, the path must not be using the character \\."""
    if file_wildcard.find('\\') != -1:
      raise InvalidPath(r"Contains unsupported character '\' :" +
                        file_wildcard)
    return ['python', '-c',
            'import chromium_utils; '
            'chromium_utils.RemoveFilesWildcards("%s")' % file_wildcard]

  def doVCUpdate(self):
    """Sync the client
    """
    dir = os.path.join(self.builder.basedir, self.srcdir)
    command = [chromium_utils.GetGClientCommand(), 'sync', '--verbose']
    # GClient accepts --revision argument of two types 'module@rev' and 'rev'.
    if self.revision:
      command.append('--revision')
      if self.branch:
        # make the revision look like branch@revision
        command.append('%s@%s' % (self.branch, self.revision))
      else:
        command.append(str(self.revision))

    c = commands.ShellCommand(self.builder, command, dir,
                              sendRC=False, timeout=self.timeout,
                              keepStdout=True)
    self.command = c
    return c.start()

  def doVCFull(self):
    """Setup the .gclient file and then sync
    """
    dir = os.path.join(self.builder.basedir, self.srcdir)
    os.mkdir(dir)
    command = [chromium_utils.GetGClientCommand(), 'config']
    
    if self.gclient_spec:
      command.append('--spec=%s' % self.gclient_spec)
    else:
      command.append(self.svnurl)
    
    c = commands.ShellCommand(self.builder, command, dir,
                              sendRC=False, timeout=self.timeout,
                              keepStdout=True)
    self.command = c
    d = c.start()
    d.addCallback(self._abandonOnFailure)
    d.addCallback(lambda _: self.doVCUpdate())
    return d

  def doClobber(self, dummy, dirname):
    """Move the old directory aside, or delete it if that's already been done.

    This function is designed to be used with a source dir.  If it's called
    with anything else, the caller will need to be sure to clean up the
    <dirname>.dead directory once it's no longer needed.

    If this is the first time we're clobbering since we last finished a
    successful update or checkout, move the old directory aside so a human
    can try to recover from it if desired.  Otherwise -- if such a backup
    directory already exists, because this isn't the first retry -- just
    remove the old directory.

    Args:
      dummy: unused
      dirname: the directory within self.builder.basedir to be clobbered
    """
    old_dir = os.path.join(self.builder.basedir, dirname)
    dead_dir = old_dir + '.dead'
    if os.path.isdir(old_dir):
      if os.path.isdir(dead_dir):
        command = self._RemoveDirectoryCommand(old_dir)
      else:
        command = self._RenameDirectoryCommand(old_dir, dead_dir)
      c = commands.ShellCommand(self.builder, command, self.builder.basedir,
                                sendRC=0, timeout=self.rm_timeout)
      self.command = c
      # See commands.SVN.doClobber for notes about sendRC.
      d = c.start()
      d.addCallback(self._abandonOnFailure)
      return d
    return None

  def doRevert(self, dummy):
    """Revert any modification done by a previous patch.

    This is done in 2 parts to trap potential errors at each step. Note that
    it is assumed that .orig and .rej files will be reverted, e.g. deleted by
    the 'gclient revert' command. If the try bot is configured with
    'global-ignores=*.orig', patch failure will occur."""
    dir = os.path.join(self.builder.basedir, self.srcdir)
    command = [chromium_utils.GetGClientCommand(), 'revert']
    c = commands.ShellCommand(self.builder, command, dir,
                              sendRC=False, timeout=self.timeout,
                              keepStdout=True)
    self.command = c
    d = c.start()
    d.addCallback(self._abandonOnFailure)
    # Remove patch residues.
    d.addCallback(lambda _: self._doRevertRemoveSignalFile())
    return d

  def _doRevertRemoveSignalFile(self):
    """Removes the file that signals that the checkout is patched.
    
    Must be called after a revert has been done and the patch residues have
    been removed."""
    command = self._RemoveFileCommand(os.path.join(self.builder.basedir,
                           self.srcdir, '.buildbot-patched'))
    dir = os.path.join(self.builder.basedir, self.srcdir)
    c = commands.ShellCommand(self.builder, command, dir,
                              sendRC=False, timeout=self.timeout,
                              keepStdout=True)
    self.command = c
    d = c.start()
    d.addCallback(self._abandonOnFailure)
    return d

  def writeSourcedata(self, res):
    """Write the sourcedata file and remove any dead source directory."""
    dead_dir = os.path.join(self.builder.basedir, self.srcdir + '.dead')
    if os.path.isdir(dead_dir):
      msg = 'Removing dead source dir'
      self.sendStatus({'header': msg + '\n'})
      log.msg(msg)
      command = self._RemoveDirectoryCommand(dead_dir)
      c = commands.ShellCommand(self.builder, command, self.builder.basedir,
                                sendRC=0, timeout=self.rm_timeout)
      self.command = c
      d = c.start()
      d.addCallback(self._abandonOnFailure)
    open(self.sourcedatafile, 'w').write(self.sourcedata)
    return res

  def parseGotRevision(self):
    """Extract the revision number from the result of the svn operation.

    svn checkout operations finish with 'Checked out revision 16657.'
    svn update operations finish the line 'At revision 16654.' when there
    is no change. They finish with 'Updated to revision 16655.' otherwise.

    """
    SVN_REVISION_RE = re.compile(
        r'^(Checked out|At|Updated to) revision ([0-9]+)\.')
    for line in self.command.stdout.splitlines():
      m = SVN_REVISION_RE.search(line)
      if m is not None:
        return int(m.group(2))
    return None

registerSlaveCommand('gclient', GClient, commands.command_version)
