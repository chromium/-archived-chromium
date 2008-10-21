#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class overload to extract a patch file and execute it on the try servers.
"""

import os
import random

from buildbot import buildset
from buildbot.changes.maildir import MaildirService, NoSuchMaildir
from buildbot.scheduler import BadJobfile, TryBase, Try_Jobdir
from buildbot.sourcestamp import SourceStamp
from twisted.application import service, internet
from twisted.python import log, runtime


class TryJob(Try_Jobdir):
  """Simple Try_Jobdir overload that executes the patch files in the pending
  directory.

  groups is the groups of default builders used for round robin. The other
  builders can still be used."""
  def __init__(self, name, pools, jobdir):
    """Skip over Try_Jobdir.__init__ to change watcher."""
    builderNames = []
    self.pools = pools
    # Flatten and remove duplicates.
    for pool in pools:
      for builder in pool:
        if not builder in builderNames:
          builderNames.append(builder)
    TryBase.__init__(self, name, builderNames)
    self.jobdir = jobdir
    self.watcher = PoolingMaildirService()
    self.watcher.setServiceParent(self)

  def parseJob(self, f):
    """Remove the NetstringReceiver bits. Only read a patch file and apply it.
    For now, hard code most of the variables.
    Add idle builder selection in each pools."""

    # The dev's email address must be hidden in that variable somehow.
    file_name = f.name
    builderNames = []
    if not file_name or len(file_name) < 2:
      buildsetID = 'Unnamed patchset'
    else:
      # Strip the path.
      file_name = os.path.basename(file_name)
      buildsetID = file_name
      # username.change_name.[bot1,bot2,bot3].diff
      items = buildsetID.split('.')
      if len(items) == 4:
        builderNames = items[2].split(",")
        print 'Choose %s for job %s' % (",".join(builderNames), file_name)

    # The user hasn't requested a specific bot. We'll choose one.
    if not builderNames:
      # First, collect the list of idle and disconnected bots.
      idle_bots = []
      disconnected_bots = []
      # self.listBuilderNames() is the merged list of 'groups'.
      for name in self.listBuilderNames():
        # parent is of type buildbot.master.BuildMaster.
        # botmaster is of type buildbot.master.BotMaster.
        # builders is a dictionary of
        #     string : buildbot.process.builder.Builder.
        if not name in self.parent.botmaster.builders:
          # The bot is non-existent.
          disconnected_bots.append(name)
        else:
          # slave is of type buildbot.process.builder.Builder
          slave = self.parent.botmaster.builders[name]
          # Get the status of the builder.
          # slave.slaves is a list of buildbot.process.builder.SlaveBuilder and
          # not a buildbot.buildslave.BuildSlave as written in the doc(nor
          # buildbot.slave.bot.BuildSlave)
          # slave.slaves[0].slave is of type buildbot.buildslave.BuildSlave
          # TODO(maruel): Support shared slave.
          if len(slave.slaves) == 1 and slave.slaves[0].slave:
            if slave.slaves[0].isAvailable():
              idle_bots.append(name)
          else:
            disconnected_bots.append(name)

      # Now for each pool, we select one bot that is idle.
      for pool in self.pools:
        prospects = []
        found_idler = False
        for builder in pool:
          if builder in idle_bots:
            # Found one bot, go for next pool.
            builderNames.append(builder)
            found_idler = True
            break
          if not builder in disconnected_bots:
            prospects.append(builder)
        if found_idler:
          continue
        # All bots are either busy or disconnected..
        if prospects:
          builderNames.append(random.choice(prospects))

      if not builderNames:
        # If no builder are available, throw a BadJobfile exception since we
        # can't select a group.
        raise BadJobfile

    print 'Choose %s for job %s' % (",".join(builderNames), file_name)

    # Always use the trunk.
    branch = None
    # Always use the latest revision, HEAD.
    baserev = None
    # The diff is the file's content.
    diff = f.read()
    # -pN argument to patch.
    patchlevel = 0

    patch = (patchlevel, diff)
    ss = SourceStamp(branch, baserev, patch)
    return builderNames, ss, buildsetID

  def messageReceived(self, filename):
    """Same as Try_Jobdir.messageReceived except 'reason' which is modified and
    the builderNames restriction lifted."""
    md = os.path.join(self.parent.basedir, self.jobdir)
    if runtime.platformType == "posix":
      # open the file before moving it, because I'm afraid that once
      # it's in cur/, someone might delete it at any moment
      path = os.path.join(md, "new", filename)
      f = open(path, "r")
      os.rename(os.path.join(md, "new", filename),
                os.path.join(md, "cur", filename))
    else:
      # do this backwards under windows, because you can't move a file
      # that somebody is holding open. This was causing a Permission
      # Denied error on bear's win32-twisted1.3 buildslave.
      os.rename(os.path.join(md, "new", filename),
                os.path.join(md, "cur", filename))
      path = os.path.join(md, "cur", filename)
      f = open(path, "r")

    try:
      builderNames, ss, bsid = self.parseJob(f)
    except BadJobfile:
      log.msg("%s reports a bad jobfile in %s" % (self, filename))
      log.err()
      return
    reason = "'%s' try job" % bsid
    bs = buildset.BuildSet(builderNames, ss, reason=reason, bsid=bsid)
    self.parent.submitBuildSet(bs)


class PoolingMaildirService(MaildirService):
  def startService(self):
    """Remove the dnotify support since it doesn't work for nfs directories."""
    self.files = {}
    service.MultiService.startService(self)
    self.newdir = os.path.join(self.basedir, "new")
    if not os.path.isdir(self.basedir) or not os.path.isdir(self.newdir):
      raise NoSuchMaildir("invalid maildir '%s'" % self.basedir)
    # Make it poll faster.
    self.pollinterval = 5
    t = internet.TimerService(self.pollinterval, self.poll)
    t.setServiceParent(self)
    self.poll()

  def poll(self):
    """Make the polling a bit more resistent by waiting two cycles before
    grabbing the file."""
    assert self.basedir
    newfiles = []
    for f in os.listdir(self.newdir):
      newfiles.append(f)
    # fine-grained timestamp in the filename
    for n in newfiles:
      try:
        stats = os.stat(os.path.join(self.newdir, f))
      except OSError:
        print 'Failed to grab stats for %s' % os.path.join(self.newdir, f)
        continue
      props = (stats.st_size, stats.st_mtime)
      if n in self.files and self.files[n] == props:
        self.messageReceived(n)
        del self.files[n]
      else:
        # Update de properties
        if n in self.files:
          print 'Last seen ' + n + ' as ' + str(props)
        print n + ' is now ' + str(props)
        self.files[n] = props
