#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Source file for buildbot.status module modifications. """

import cgi
import os
import re
import urllib

import buildbot.status.web.base as base
import buildbot.status.web.baseweb as baseweb
import buildbot.status.web.waterfall as waterfall
import buildbot.status.web.changes as changes
import buildbot.status.web.builder as builder
import buildbot.status.web.slaves as slaves
import buildbot.status.web.xmlrpc as xmlrpc
import buildbot.status.web.about as about
from buildbot.status.web.base import make_row
import buildbot.status.builder as statusbuilder
import buildbot.status.words as words
import buildbot.sourcestamp as sourcestamp

from twisted.python import log
from twisted.python import components
from twisted.web.util import Redirect
from twisted.web import html
from zope.interface import interface
from zope.interface import declarations

import chromium_config as config
import chromium_process
import irc_contact

# Global that is used by status objects to pickup IRC topic.
DEFAULT_IRC_TOPIC = config.IRC.default_topic
current_irc_topic = DEFAULT_IRC_TOPIC
# Values that represent builder status.
OPEN, CLOSED, UNKNOWN = range(3)
# Constant that is used by TreeStatus to indicate all the builders.
ALL_BUILDERS = 'ALL_BUILDERS'
# Name of an attribute that we dynamically set on Builder object
# to represent its status and it can be one of OPEN, CLOSED, UNKNOWN constant
# values. IrcStatusBot sets the value after interpreting topic on monitored
# IRC channel and then waterfall uses the value to render the builder boxes.
BUILDER_TREE_STATUS_ATTRIBUTE = 'tree_status'


class IrcStatusBot(words.IrcStatusBot):

  def __init__(self, *args):
    words.IrcStatusBot.__init__(self, *args)
    # container for all the closed builder names that are identified
    # by the _ReflectTopicNameOnBuilders method.
    self._closed_builder_names = set()
    for builder in self.status.botmaster.builders.values():
      setattr(builder, BUILDER_TREE_STATUS_ATTRIBUTE, UNKNOWN)

  def topicUpdated(self, user, channel, topic):
    global current_irc_topic
    current_irc_topic = topic
    self._ReflectTopicNameOnBuilders(topic, channel)
    self._AnnounceClosedBuilders(channel)

  def _AnnounceClosedBuilders(self, channel):
    """Announces names of closed builders on all available IRC channels."""
    if len(self._closed_builder_names) == 0:
      return
    if len(self._closed_builder_names) == 1:
      announcement = "%s is closed now!" % self._closed_builder_names.pop()
    else:
      announcement = ('%s are closed now!'
                      % ', '.join(self._closed_builder_names))

    self.say(channel, announcement)

  def _ReflectTopicNameOnBuilders(self, topic, channel):
    """Identifies which project trees are closed and sets
    BUILDER_TREE_STATUS_ATTRIBUTE attribute on each
    buildbot.process.builer.Builder instances any of OPEN, CLOSED, UNKNOWN
    values.

    If there is no connection it is set to UNKNOWN. If the closed project name
    matches the prefix of the builder name it is set as CLOSED. Otherwise we
    have an OPEN builder.
    """
    class TreeState:
      SCOPE, STATEMENT, STATUS, END_OF_STATEMENT, IGNORE = range(5)
      """ Class that provides some heuristical logic to find out what projects
      are declared as closed.
      """
      negative_scope = ['except', 'besides', 'but']
      positive_scope = ['all', 'everything']
      # 'Tree' counts as 'everything' only if no project has been seen, and is
      # ignored otherwise.  This allows 'tree is closed' as well as
      # 'chrome tree is closed'.
      tree_scope = ['tree']
      scope = negative_scope + positive_scope
      statement = ['is', 'are']
      status = ['closed', 'open']
      end_of_statement = ['.', ';'] # and last word except status types

      def __init__(self, phrase):
        self.__words = self._Tokenize(phrase.lower())

      def ClosedTrees(self):
        """ Heuristically identifies what projects are closed.

        Can return [ALL_BUILDERS] if all the projects are closed.
        Currently we recognize statements such as:
        'tree is closed',
        'all trees are closed',
        'chrome tree is closed',
        'chrome, webkit and v8 are closed',
        'chrome and webkit are closed'
        'projectname is closed and rest are open. Coffee time.',
        'proj1, proj2 are closed and rest are open',
        'everything is open except webkit',
        'everything is open except proj1 and proj2'

        'everything is closed except webkit' approach is not supported.
        """
        closed_projects = []
        current_projects = []

        consider_everything = False
        maybe_rest_are_closed = False
        previous_status = None
        word_index = 0

        for word in self.__words:
          word_index +=1
          is_last_word = (word_index == len(self.__words))
          has_projects = (len(current_projects) > 0)
          word_type = self._IdentifyType(word, has_projects, is_last_word)

          if word_type == TreeState.SCOPE:
            if word in self.negative_scope:
              current_projects = []
              consider_everything = False
              if previous_status == 'open':
                maybe_rest_are_closed = True
            elif word in self.positive_scope or word in self.tree_scope:
              consider_everything = True

          elif word_type == TreeState.STATEMENT:
            if word == 'is' and len(current_projects) > 1:
              current_projects = current_projects[-1:]

          elif word_type == TreeState.STATUS:
            if word == 'closed':
              if consider_everything:
                closed_projects = [ALL_BUILDERS]
                continue
              closed_projects.extend(current_projects)
            current_projects = []
            consider_everything = False
            previous_status = word

          elif word_type == TreeState.END_OF_STATEMENT:
            if maybe_rest_are_closed:
              if not word in self.end_of_statement:  #last word
                current_projects.append(word)
              if len(current_projects):
                closed_projects.extend(current_projects)
            current_projects = []
            consider_everything = False

          elif word_type != TreeState.IGNORE:
            current_projects.append(word)

        return closed_projects

      def _IdentifyType(self, word, has_projects, is_last_word):
        """Finds out what class of word we are processing."""
        word_type = None
        if word in self.scope:
          word_type = TreeState.SCOPE
        elif word in self.statement:
          word_type = TreeState.STATEMENT
        elif word in self.status:
          word_type = TreeState.STATUS
        elif word in self.end_of_statement or is_last_word:
          word_type = TreeState.END_OF_STATEMENT
        elif word in self.tree_scope:
          if has_projects:
            word_type = TreeState.IGNORE
          else:
            word_type = TreeState.SCOPE
        return word_type

      def _Tokenize(self, phrase):
        """Splits phrase into words. Recognizes some punctuation."""
        words = []
        punctuation = [',', '.', ';', '!', ':']
        current_word = []

        for character in phrase:
          if character.isspace() or punctuation.count(character) > 0:
            if len(current_word) > 0:
              words.append(''.join(current_word))
              current_word = []
            if punctuation.count(character) > 0:
              words.append(character)
          else:
            current_word.append(character)
        # store the last word
        if len(current_word) > 0:
          words.append(''.join(current_word))
        return words

    closed_trees = TreeState(topic).ClosedTrees()
    self._closed_builder_names = set() # reset closed builder names
    for buildername in self.status.botmaster.builders:
      builder = self.status.botmaster.builders[buildername]
      should_close = False
      if ALL_BUILDERS in closed_trees:
        should_close = True
        self._closed_builder_names = set(['everything'])
      else:
        for prefix in closed_trees:
          if buildername[:len(prefix)] == prefix:
             should_close = True
             self._closed_builder_names.add(prefix + '*')
             break
      if should_close:
        setattr(builder, BUILDER_TREE_STATUS_ATTRIBUTE, CLOSED)
      else:
        setattr(builder, BUILDER_TREE_STATUS_ATTRIBUTE, OPEN)


class IrcStatusChatterBot(IrcStatusBot):
  """Provides additional chat responses and interaction."""
  # Only one line of this method, noted, has been changed from the base
  # words.IrcStatusBot behavior.
  def privmsg(self, user, channel, message):
    user = user.split('!', 1)[0] # rest is ~user@hostname
    # channel is '#twisted' or 'buildbot' (for private messages)
    channel = channel.lower()
    #print "privmsg:", user, channel, message
    if channel == self.nickname:
      # private message
      contact = self.getContact(user)
      contact.handleMessage(message, user)
      return
    # else it's a broadcast message, maybe for us, maybe not. 'channel'
    # is '#twisted' or the like.
    contact = self.getContact(channel)
    # Change: look at all messages, even ones that aren't to the buildbot.
    contact.handleMessage(message, user)

  # Only one line of this method, noted, has been changed from the base
  # words.IrcStatusBot behavior.
  def getContact(self, name):
    if name in self.contacts:
      return self.contacts[name]
    # Change: instantiate our IRCContact instead of the base one in words.
    new_contact = irc_contact.IRCContact(self, name, self.nickname)
    self.contacts[name] = new_contact
    return new_contact

  # Only one line of this method, noted, has been changed from the base
  # words.IrcStatusBot behavior.
  def action(self, user, channel, data):
    #log.msg("action: %s,%s,%s" % (user, channel, data))
    user = user.split('!', 1)[0] # rest is ~user@hostname
    # somebody did an action (/me actions) in the broadcast channel
    contact = self.getContact(channel)
    # Change: look for our nickname rather than hard-coded "buildbot".
    if self.nickname in data:
      contact.handleAction(data, user)

  # Adds to the parent IRCStatusBot's behavior (see above) to pass the user and
  # topic to the contact.
  def topicUpdated(self, user, channel, topic):
    global current_irc_topic
    # Don't tell the contact if we've just started up.
    if current_irc_topic != DEFAULT_IRC_TOPIC:
      contact = self.getContact(channel)
      contact.TopicChanged(user, topic)
    current_irc_topic = topic
    self._ReflectTopicNameOnBuilders(topic, channel)
    self._AnnounceClosedBuilders(channel)


class CurrentBox(waterfall.CurrentBox):
  """Adds "CLOSED" indicator to the waterfall display for closed builders."""
  def getBox(self, status):
    box = waterfall.CurrentBox.getBox(self, status)

    builderName = self.original.getName()
    builder = status.botmaster.builders[builderName]
    if (hasattr(builder, BUILDER_TREE_STATUS_ATTRIBUTE)
                and getattr(builder, BUILDER_TREE_STATUS_ATTRIBUTE) == CLOSED):
      box.text.append('<div class="large">CLOSED</div>')
      box.class_ = 'closed'
    return box

# First unregister ICurrentBox registered by waterfall.py.
# We won't be able to unregister it without messing with ALLOW_DUPLICATES
# in twisted.python.components. Instead, work directly with adapter to
# remove the component:
origInterface = statusbuilder.BuilderStatus
origInterface = declarations.implementedBy(origInterface)
registry = components.getRegistry()
# setting to None does the trick.
registry.register([origInterface], base.ICurrentBox, '', None)
# Finally, register our CurrentBox:
components.registerAdapter(CurrentBox, statusbuilder.BuilderStatus,
                            base.ICurrentBox)

class BuildBox(waterfall.BuildBox):
  """Build the yellow starting-build box for a waterfall column.

  This subclass adds the builder's name to the box.  It's otherwise identical
  to the parent class, apart from naming syntax.
  """
  def getBox(self, req):
    b = self.original
    number = b.getNumber()
    url = base.path_to_build(req, b)
    reason = b.getReason()
    if reason:
      text = (('%s<br><a href="%s">Build %d</a><br>%s')
              % (b.getBuilder().getName(), url, number, html.escape(reason)))
    else:
      text = ('%s<br><a href="%s">Build %d</a>'
              % (b.getBuilder().getName(), url, number))
    color = "yellow"
    class_ = "start"
    if b.isFinished() and not b.getSteps():
      # the steps have been pruned, so there won't be any indication
      # of whether it succeeded or failed. Color the box red or green
      # to show its status
      color = b.getColor()
      class_ = base.build_get_class(b)
    return base.Box([text], color=color, class_="BuildStep " + class_)

# See comments for re-registering ICurrentBox.
origInterface = statusbuilder.BuildStatus
origInterface = declarations.implementedBy(origInterface)
registry.register([origInterface], base.IBox, '', None)
components.registerAdapter(BuildBox, statusbuilder.BuildStatus, base.IBox)

class HorizontalOneBoxPerBuilder(base.HtmlResource):
    """This shows a table with one cell per build. The color of the cell is
    the state of the most recently completed build. If there is a build in
    progress, the ETA is shown in table cell. The table cell links to the page
    for that builder. They are layed out, you guessed it, horizontally.

    builder=: show only builds for this builder. Multiple builder= arguments
              can be used to see builds from any builder in the set. If no
              builder= is given, shows them all.
    """

    def body(self, request):
      status = self.getStatus(request)
      builders = request.args.get("builder", status.getBuilderNames())

      data = "<table style='width:100%'><tr>"

      for builder_name in builders:
        builder = status.getBuilder(builder_name)
        classname = base.ITopBox(builder).getBox(request).class_
        title = builder_name

        builder_status = status.botmaster.builders[builder_name]
        if (hasattr(builder_status, BUILDER_TREE_STATUS_ATTRIBUTE)
            and getattr(builder_status,
                        BUILDER_TREE_STATUS_ATTRIBUTE) == CLOSED):
          classname += ' mini-closed'
          title = "CLOSED: " + title

        url = (self.path_to_root(request) + "waterfall?builder=" +
               urllib.quote(builder_name, safe=''))
        link = '<a href="%s" class="%s" title="%s" \
            target=_blank> </a>' % (url, classname, title)
        data += '<td valign=bottom class=mini-box>%s</td>' % link

      data += "</tr></table>"

      global current_irc_topic
      escaped_topic = cgi.escape(current_irc_topic)

      # If we're just loading the status-summary page, remove margins and set
      # the title to the IRC topic.
      data += "<script> \
          if (top.location.toString().match(/status-summary\.html$/)) { \
            document.body.style.marginLeft=0; \
            document.body.style.marginRight=0; \
            top.document.title='%s'; \
          }</script>" % escaped_topic

      return data


class WaterfallStatusResource(waterfall.WaterfallStatusResource):
  """Class that overrides default behavior of
  waterfall.WaterfallStatusResource. """

  ANNOUNCEMENT_FILE = 'public_html/announce.html'
  SUMMARY_FILE = 'public_html/status-summary.html'

  DEFAULT_REFRESH_TIME = 60

  def head(self, request):
    """ Adds META to refresh page by specified value in the request,
    sets to one minute by default. """
    reload_time = self.get_reload_time(request)
    if reload_time is None:
      reload_time = WaterfallStatusResource.DEFAULT_REFRESH_TIME
    return '<meta http-equiv="refresh" content="%d">\n' % reload_time

  def body(self, request):
    """Calls default body method and prepends Tree Status HTML based on
    IRC topic."""

    data = waterfall.WaterfallStatusResource.body(self, request)

    return "%s %s" % (self.__TreeStatus(), data)

  def __TreeStatus(self):
    """Creates DIV that provides visuals on tree status.

    """
    data = '<div class="Announcement">\n'
    data += '<iframe width="100%" height="60" frameborder="0" scrolling="no" src="http://chromium-status.appspot.com/current"></iframe>\n' 
    data += self.__GetStaticFileContent(
        WaterfallStatusResource.ANNOUNCEMENT_FILE)
    data += '</div>\n'
    data += self.__GetStaticFileContent(WaterfallStatusResource.SUMMARY_FILE)

    return data

  def __GetStaticFileContent(self, file):
    if os.path.exists(file):
      return open(file).read().strip()
    else:
      return ''

class StatusResourceBuilder(builder.StatusResourceBuilder):
  """Class that overrides default behavior of builders.StatusResourceBuilder.

  The reason for that is to expose additional HTML controls and
  set BuildRequest custom attributes we need to add.
  """

  def body(self, req):
    """ Overriden method from builders.StatusResourceBuilder. The only
    change in original behavior is added new checkbox for clobbering."""
    b = self.builder_status
    control = self.builder_control
    status = self.getStatus(req)

    slaves = b.getSlaves()
    connected_slaves = [s for s in slaves if s.isConnected()]

    projectName = status.getProjectName()

    data = '<a href="%s">%s</a>\n' % (self.path_to_root(req), projectName)

    data += "<h1>Builder: %s</h1>\n" % html.escape(b.getName())

    # the first section shows builds which are currently running, if any.

    current = b.getCurrentBuilds()
    if current:
      data += "<h2>Currently Building:</h2>\n"
      data += "<ul>\n"
      for build in current:
        data += " <li>" + self.build_line(build, req) + "</li>\n"
      data += "</ul>\n"
    else:
      data += "<h2>no current builds</h2>\n"

    # Then a section with the last 5 builds, with the most recent build
    # distinguished from the rest.

    data += "<h2>Recent Builds:</h2>\n"
    data += "<ul>\n"
    for i,build in enumerate(b.generateFinishedBuilds(num_builds=5)):
      data += " <li>" + self.make_line(req, build, False) + "</li>\n"
      if i == 0:
        data += "<br />\n" # separator
        # TODO: or empty list?
    data += "</ul>\n"


    data += "<h2>Buildslaves:</h2>\n"
    data += "<ol>\n"
    for slave in slaves:
      name = slave.getName()
      if not name:
        name = ''
      data += "<li><b>%s</b>: " % html.escape(name)
      if slave.isConnected():
        data += "CONNECTED\n"
        if slave.getAdmin():
          data += make_row("Admin:", html.escape(slave.getAdmin()))
        if slave.getHost():
          data += "<span class='label'>Host info:</span>\n"
          data += html.PRE(slave.getHost())
      else:
        data += ("NOT CONNECTED\n")
      data += "</li>\n"
    data += "</ol>\n"

    if control is not None and connected_slaves:
      forceURL = urllib.quote(req.childLink("force"))
      data += (
        """
        <form action='%(forceURL)s' class='command forcebuild'>
        <p>To force a build, fill out the following fields and
        push the 'Force Build' button</p>
        <table>
          <tr class='row' valign="bottom">
            <td>
              <span class="label">Your name<br>
               (<span style="color: red;">please fill in at least this
               much</span>):</span>
            </td>
            <td>
              <span class="field"><input type='text' name='username' /></span>
            </td>
          </tr>
          <tr class='row'>
            <td>
              <span class="label">Reason for build:</span>
            </td>
            <td>
              <span class="field"><input type='text' name='comments' /></span>
            </td>
          </tr>
          <tr class='row'>
            <td>
              <span class="label">Branch to build:</span>
            </td>
            <td>
              <span class="field"><input type='text' name='branch' /></span>
            </td>
          </tr>
          <tr class='row'>
            <td>
              <span class="label">Revision to build:</span>
            </td>
            <td>
              <span class="field"><input type='text' name='revision' /></span>
            </td>
          </tr>
          <tr class='row'>
            <td>
              <span class="label">Clobber:</span>
            </td>
            <td>
              <span class="field"><input type='checkbox' name='clobber' />
              </span>
            </td>
          </tr>
        </table>
        <input type='submit' value='Force Build' />
        </form>
        """) % {"forceURL": forceURL}
    elif control is not None:
      data += """
      <p>All buildslaves appear to be offline, so it's not possible
      to force this build to execute at this time.</p>
      """

    if control is not None:
      pingURL = urllib.quote(req.childLink("ping"))
      data += """
      <form action="%s" class='command pingbuilder'>
      <p>To ping the buildslave(s), push the 'Ping' button</p>

      <input type="submit" value="Ping Builder" />
      </form>
      """ % pingURL

    return data

  def force(self, req):
    """ Overriden method from builders.StatusResourceBuilder. The only
    change to original behavior is that it sets 'clobber' value on the
    BuildRequest."""

    name = req.args.get("username", ["<unknown>"])[0]
    reason = req.args.get("comments", ["<no reason specified>"])[0]
    branch = req.args.get("branch", [""])[0]
    revision = req.args.get("revision", [""])[0]
    clobber = req.args.get("clobber", [False])[0]

    if clobber:
      clobber_str = ' (clobbered)'
    else:
      clobber_str = ''
    r = ("The web-page 'force build' button%s was pressed by %s: %s" %
        (clobber_str, name, reason))

    message = ("web forcebuild of builder '%s', branch='%s', revision='%s'"
               ", clobber='%s'")
    log.msg(message
        % (self.builder_status.getName(), branch, revision, clobber))

    if not self.builder_control:
      # TODO: tell the web user that their request was denied
      log.msg("but builder control is disabled")
      return Redirect("..")

    # keep weird stuff out of the branch and revision strings. TODO:
    # centralize this somewhere.
    if not re.match(r'^[\w\.\-\/]*$', branch):
      log.msg("bad branch '%s'" % branch)
      return Redirect("..")
    if not re.match(r'^[\w\.\-\/]*$', revision):
      log.msg("bad revision '%s'" % revision)
      return Redirect("..")
    if branch == "":
      branch = None
    if revision == "":
      revision = None

    # TODO: if we can authenticate that a particular User pushed the
    # button, use their name instead of None, so they'll be informed of
    # the results.
    s = sourcestamp.SourceStamp(branch=branch, revision=revision)
    req = chromium_process.BuildRequest(r, s, self.builder_status.getName(),
                                      clobber=clobber)
    try:
      self.builder_control.requestBuildSoon(req)
    except interfaces.NoSlaveError:
      # TODO: tell the web user that their request could not be
      # honored
      pass
    return Redirect("../..")

class BuildersResource(builder.BuildersResource):
  """ Overrides instantiated builder.StatusResourceBuilder class with our
  custom implementation.

  Ideally it would be nice to expose builder.StatusResourceBuilder on
  WebStatus. Unfortunately BuildersResource is the one that defines
  builder.StatusResourceBuilder as its child resource.
  """
  def getChild(self, path, req):
    s = self.getStatus(req)
    if path in s.getBuilderNames():
      builder_status = s.getBuilder(path)
      builder_control = None
      c = self.getControl(req)
      if c:
        builder_control = c.getBuilder(path)
      # We return chromium_status.StatusResourceBuilder instance here. The code
      # is exactly the same and just having the code in this source file makes
      # it instantiate chromium_status.StatusResourceBuilder
      return StatusResourceBuilder(builder_status, builder_control)

    return base.HtmlResource.getChild(self, path, req)


class WebStatus(baseweb.WebStatus):
  """Class that provides hook to modify default behavior of
  baseweb.WebStatus.

  If you wish to override web status classes, this is the place
  to instantiate them.
  """

  def setupUsualPages(self):
    """ The method that defines resources to expose.

    We are supplying our modified WaterfallStatusResource and BuildersResource
    instances.
    """
    # baseweb.putChild() just records the putChild() calls and doesn't
    # actually add them to the server until initialization is complete.
    # So call the superclass to get the default pages supplied by
    # the current version of buildbot, and then just override our
    # custom ones.
    baseweb.WebStatus.setupUsualPages(self)

    self.putChild("waterfall", WaterfallStatusResource())
    self.putChild("builders", BuildersResource())
    self.putChild("horizontal_one_box_per_builder",
                  HorizontalOneBoxPerBuilder())
