#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from buildbot.changes import svnpoller
from buildbot.changes.changes import Change


class SVNPoller(svnpoller.SVNPoller):
  """Extension of svnpoller.SVNPoller that provides workarounds for
    the GVN incompatibility."""

  def create_changes(self, new_logentries):
    """Overriden svnpoller.SVNPoller.create_changes that ignores path
    that starts with '/changes/'.
    """
    changes = []
    for el in new_logentries:
      branch_files = [] # get oldest change first
      # TODO: revisit this, I think I've settled on Change.revision
      # being a string everywhere, and leaving the interpretation
      # of that string up to b.s.source.SVN methods
      revision = int(el.getAttribute("revision"))
      svnpoller.dbgMsg("Adding change revision %s" % (revision,))
      # TODO: the rest of buildbot may not be ready for unicode 'who'
      # values
      author   = self._get_text(el, "author")
      comments = self._get_text(el, "msg")
      # there is a "date" field, but it provides localtime in the
      # repository's timezone, whereas we care about buildmaster's
      # localtime (since this will get used to position the boxes on
      # the Waterfall display, etc). So ignore the date field and use
      # our local clock instead.
      #when     = self._get_text(el, "date")
      #when     = time.mktime(time.strptime("%.19s" % when,
      #                                     "%Y-%m-%dT%H:%M:%S"))
      branches = {}
      pathlist = el.getElementsByTagName("paths")[0]
      for p in pathlist.getElementsByTagName("path"):
        path = "".join([t.data for t in p.childNodes])
        if self.__ShouldIgnorePath(path):
          continue
        # the rest of buildbot is certaily not yet ready to handle
        # unicode filenames, because they get put in RemoteCommands
        # which get sent via PB to the buildslave, and PB doesn't
        # handle unicode.
        path = path.encode("ascii")
        if path.startswith("/"):
          path = path[1:]
        where = self._transform_path(path)
        # if 'where' is None, the file was outside any project that
        # we care about and we should ignore it
        if where:
          branch, filename = where
          if not branch in branches:
            branches[branch] = []
          branches[branch].append(filename)

      for branch in branches:
        c = Change(who=author,
                   files=branches[branch],
                   comments=comments,
                   revision=revision,
                   branch=branch)
        changes.append(c)

    return changes

  def __ShouldIgnorePath(self, path):
    # Assertions fail on 'D /changes/...' path since it is not typical SVN path
    # It represents changeset in GVN that is deleted on check-in and should
    # be ingored.
    return path.startswith('/changes/')
