#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to mail the try bot results.
"""

from buildbot.status import mail
from buildbot.status.builder import FAILURE, SUCCESS, WARNINGS
from email.Message import Message
from email.Utils import formatdate
import urllib
from twisted.internet import defer


class MailNotifier(mail.MailNotifier):
  def buildMessage(self, name, build, results):
    """Send an email about the result. Don't attach the patch as
    MailNotifier.buildMessage do."""

    projectName = self.status.getProjectName()
    ss = build.getSourceStamp()
    build_url = self.status.getURLForThing(build)
    waterfall_url = self.status.getBuildbotURL()
    # build.getSlavename()

    patch_url_text = ""
    if ss and ss.patch:
      patch_url_text = "Patch: %s/steps/gclient/logs/patch\n\n" % build_url
    failure_bot = ""
    failure_tree = ""
    if results != SUCCESS:
      failure_bot = "(In case the bot is broken)\n"
      failure_tree = "(in case the tree is broken)\n"

    if ss is None:
      source = "unavailable"
    else:
      source = ""
      if ss.branch:
        source += "[branch %s] " % ss.branch
      if ss.revision:
        source += ss.revision
      else:
        source += "HEAD" # TODO(maruel): Tell the exact rev? That'd require feedback from the bot :/
      if ss.patch is not None:
        source += " (plus patch)"

    t = build.getText()
    if t:
      t = ": " + " ".join(t)
    else:
      t = ""

    if results == SUCCESS:
      status_text = "You are awesome! Try succeeded!"
      res = "success"
    elif results == WARNINGS:
      status_text = "Try Had Warnings%s" % t
      res = "warnings"
    else:
      status_text = "TRY FAILED%s" % t
      res = "failure"
      
    text = """Slave: %s

Run details: %s

%sSlave history: %swaterfall?builder=%s
%s
Build Reason: %s

Build Source Stamp: %s

--=>  %s  <=--

Try server waterfall: %s
Buildbot waterfall: http://build.chromium.org/
%s
Sincerely,

 - The monkey which sends all these emails manually


Reminders:
 - Patching is done inside the update step.
 - So if the update step failed, that may be that the patch step failed.
   The patch can fail because of svn property change or binary file change.
 - The patch will fail if the tree has conflicting changes since your last upload.
 - On Windows, the execution order is compile debug, test debug, compile release.
 - On Mac/linux, the execution order is compile debug, test debug.

Automated text version 0.2""" % (name,
       build_url,
       patch_url_text,
       urllib.quote(waterfall_url, '/:'),
       urllib.quote(name),
       failure_bot,
       build.getReason(),
       source,
       status_text,
       urllib.quote(waterfall_url, '/:'),
       failure_tree)

    m = Message()
    m.set_payload(text)

    m['Date'] = formatdate(localtime=True)
    m['Subject'] = self.subject % {
      'result': res,
      'projectName': projectName,
      'builder': name,
      'reason': build.getReason(),
    }
    m['From'] = self.fromaddr
    # now, who is this message going to?
    dl = []
    recipients = self.extraRecipients[:]
    if self.sendToInterestedUsers and self.lookup:
      for u in build.getInterestedUsers():
        d = defer.maybeDeferred(self.lookup.getAddress, u)
        d.addCallback(recipients.append)
        dl.append(d)
    d = defer.DeferredList(dl)
    d.addCallback(self._gotRecipients, recipients, m)
    return d
