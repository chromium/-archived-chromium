#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""IRCContact subclass and supporting class for a more personable IRC buildbot.
"""

import datetime
import errno
import os
import pytz
import random
import re
import sys
import urlparse

import json_file

from twisted.internet import reactor
from twisted.python import log
from twisted.python import failure

import buildbot.status.words as words

import chromium_config as configuration

DEBUGGING = False

# The data lists are global so they can be shared among all instances of the
# IRCContact -- that is, the main channel and all the private chats.  This
# lets them be reloaded in a private chat and apply to the main channel too.
_config = {'custom_help': {},
           'unknown_command': [],
           'global_methods': [],
           'admin_methods': [],
           'message_methods': [],
           'message_replies': [],
           'action_replies': [],
          }

def RandomChoice(choices, default=''):
  """Returns a random selection from the sequence of choices, or the default
  value if the sequence is empty.
  """
  try:
    choice = random.choice(choices)
  except IndexError:
    choice = default
  return choice


def IsPrivateMessage(dest):
  """Returns true if the 'dest' channel is a private message (as opposed to a
  broadcast channel).
  """
  return not dest.startswith('#')


# If set to False by config or an admin command, the bot won't do any of its
# special responses.  It's not quite a conventional buildbot at that point,
# but it's close.
_bot_enabled = True

# Enumerated constants describing whether the query we're replying to was
# plain text or a "/me" action.
(TYPE_MESSAGE, TYPE_ACTION) = (0, 1)

# Usernames of people allowed to execute administrative commands.
ADMINISTRATORS = configuration.IRC.bot_admins


class Responder(object):
  """Examines incoming queries and determines appropriate replies or responses.
  """
  # This file holds patterns and replies for the bot, as eval-able Python. The
  # file should be located somewhere in the module search path (for instance,
  # next to this module).  The patterns are stored in a separate file so they
  # can be reloaded without modifying the bot.
  BOT_DATA_FILE = 'bot_data.py'

  # Dynamic data files holding jokes (in plain JSON format) and whuffie data
  # (as a fully valid JavaScript statement).
  JOKE_FILE = 'joke_list.json'  # relative to the current directory
  # Because they will be served as part of web pages, the whuffie and topic
  # data files live in the bot-user's www dir.
  WHUFFIE_FILE = os.path.expanduser(configuration.IRC.whuffie_file)
  WHUFFIE_REASON_FILE = os.path.expanduser(
      configuration.IRC.whuffie_reason_file)
  TOPIC_FILE = os.path.expanduser(configuration.IRC.topic_file)

  # Default joke if the list is empty, time after which the most recent joke
  # can no longer be deleted, and text marker for deleted jokes.
  DEFAULT_JOKE = "I can't think of any right now."
  JOKE_TIMEOUT = datetime.timedelta(minutes=5)
  JOKE_DELETED_MARKER = 'DELETED '

  # Regular expression for finding whuffie adjustments.
  WHUFFIE_REGEXP = re.compile(r'(?u)([\w]+)(\+\+|--)')

  def __init__(self, dest, nickname, joke_file=None, whuffie_file=None,
               whuffie_reason_file=None, topic_file=None):
    """
    Args:
      dest: As provided by the IRC Channel class when it creates an IRCContact
          and passed along to this Responder in turn, this is a string of the
          form '#name' (for a chat channel) or 'name' (for a private message).
      nickname: this bot's chat nickname
      joke_file: path to the joke data file, absolute or relative to the
          current directory; defaults to Responder.JOKE_FILE
      whuffie_file: path to the whuffie data file, absolute or relative to the
          current directory; defaults to Responder.WHUFFIE_FILE
      whuffie_reason_file: path to the file holding whuffie reasons, absolute
          or relative to the current directory; defaults to
          Responder.WHUFFIE_REASON_FILE
      topic_file: path to the IRC topic data file, absolute or relative to the
          current directory; defaults to Responder.TOPIC_FILE
    """
    self._dest = dest
    self._nickname = nickname.lower()

    joke_file = joke_file or Responder.JOKE_FILE
    whuffie_file = whuffie_file or Responder.WHUFFIE_FILE
    whuffie_reason_file = whuffie_reason_file or Responder.WHUFFIE_REASON_FILE
    topic_file = topic_file or Responder.TOPIC_FILE

    # Timezones for conversion.
    self._timezones = { 'utc': pytz.timezone('UTC'),
                        'beijing': pytz.timezone('Asia/Shanghai'),
                        'dk': pytz.timezone('Europe/Copenhagen'),
                        'jp': pytz.timezone('Asia/Tokyo'),
                        'mtv': pytz.timezone('US/Pacific'),
                        'nyc': pytz.timezone('US/Eastern'),
                       }

    self._joke_list = json_file.UniqueListDataFile(joke_file)
    self._whuffie_list = json_file.DictValueDataFile(whuffie_file, mode=0644,
                                                     varname='whuffie_data')
    self._whuffie_reasons = json_file.UniqueListDataFile(whuffie_reason_file,
                                                     size=30,
                                                     mode=0644,
                                                     varname='whuffie_reasons')
    self._topic_list = json_file.UniqueListDataFile(topic_file, size=100,
                                                    mode=0644,
                                                    varname='topic_data')

    # Track the last joke we told, so we don't repeat it and so it can be
    # removed.
    self._last_joke = None
    self._last_joke_time = datetime.datetime.min

    # Track the user and time of the last topic change.
    self._topic_when = datetime.datetime.min
    self._topic_who = ''

    # Fill configuration lists from the BOT_DATA_FILE.
    self._ImportData()

  #
  # Public methods
  #

  def FindReply(self, message, who, type):
    """If the message matches a query in the reply list for the given message
    type, returns a random reply for that query, else returns None.

    Args:
      message: the message sent
      who: which user sent it
      type: one of the enumerated constants describing the type of message
    """
    if type == TYPE_MESSAGE:
      reply_list = _config['message_replies']
    elif type == TYPE_ACTION:
      reply_list = _config['action_replies']
    else:
      return None

    for response in reply_list:
      regexp = response[0]
      if regexp.search(message):
        return self._MakeSubstitutions(RandomChoice(response[1]), who)
    return None

  def FindReplyMethodToAnyone(self, message, who):
    """Returns the matching global method for the message, or None."""
    return self._FindReplyMethod(message, who, _config['global_methods'])

  def FindReplyMethodFromAdmin(self, message, who):
    """Returns the matching admin method for the message, or None."""
    return self._FindReplyMethod(message, who, _config['admin_methods'])

  def FindReplyMethodToMe(self, message, who):
    """Returns the matching general method for the message, or None."""
    return self._FindReplyMethod(message, who, _config['message_methods'])

  def UnknownCommandReplies(self):
    """Returns the list of replies to an unknown command."""
    return _config['unknown_command']

  def TopicUpdated(self, user, topic):
    """Records who set the topic, and when."""
    self._topic_who = user
    self._topic_when = datetime.datetime.now()
    when = self._FormatTime(self._topic_when, short=True)
    self._topic_list.Append('%s (%s): %s' % (when, user, self._Linkify(topic)))

  #
  # Response methods
  #

  def _GiveHelp(self, message, who, match):
    """Returns help for the command, if any, or a general help message."""
    command = match.group(1)
    if command:
      reply = self._CustomHelp(command.strip())
      if reply:
        return reply
    return ('Try "help <command>". Help is available for the following '
            'commands: %s' % ', '.join(self._CustomHelpList()))

  def _DescribeTopic(self, message, who, match):
    """Reports when the current topic was set and by whom, since some clients
    (e.g. Colloquy 2.1 for Mac) can't display this on their own.
    """
    if self._topic_when == datetime.datetime.min:
      return 'The topic was set before I woke up.'
    when = self._FormatTime(self._topic_when)
    who = self._topic_who or "somebody I don't remember"
    return 'The topic was set at %s, by %s.' % (when, who)

  def _TellJoke(self, message, who, match):
    """Returns a random joke from the current list.

    The last joke told is tracked so it can be removed and so it isn't
    repeated immediately.  The time is also recorded, to time out removals.
    """
    # Try a few times to find a different joke from last time.
    jokes = [x for x in self._joke_list.GetList()
             if not x.startswith(Responder.JOKE_DELETED_MARKER)]
    for tries in xrange(5):
      new_joke = RandomChoice(jokes, Responder.DEFAULT_JOKE)
      if new_joke != self._last_joke:
        break
    self._last_joke = new_joke
    self._last_joke_time = datetime.datetime.now()
    return new_joke

  def _AddJoke(self, message, who, match):
    """Adds a joke to the current list and returns a reply."""
    joke = match.group(2)
    if len(joke) > 1:
      self._joke_list.Append(joke)
      self._last_joke = joke
      self._last_joke_time = datetime.datetime.now()
      return "That's a good one."
    return None

  def _RemoveJoke(self, message, who, match):
    """Marks as removed the last joke that was added or told.

    A joke marked as removed won't be told anymore, but it stays in the list
    with a text marker prepended until an admin purges the list.  If it's
    added again, both copies stay in the list; but if it's then removed a
    second time, only one removed copy is present, so it won't reproduce
    indefinitely.

    If the last joke was added or told too long ago, or if it's already been
    removed, gives a suitable reply.
    """
    if (datetime.datetime.now() - self._last_joke_time >
        Responder.JOKE_TIMEOUT):
      self._last_joke = None

    # The last joke is None if it's been too long since we told one, or '' if
    # the previous joke has already been deleted.
    if self._last_joke is None:
      return "I haven't told any jokes recently."
    if self._last_joke == '':
      return "It's already gone."

    self._joke_list.Update(self._last_joke,
                           Responder.JOKE_DELETED_MARKER + self._last_joke)
    self._last_joke = ''
    return "You're right, that one isn't very funny."

  def _FindWhuffieAdjustments(self, message, who, match):
    """Finds and applies each whuffie match in the message in turn.  Returns
    the first of any replies generated.

    Anonymous whuffie (i.e., in private messages) isn't permitted.
    """
    if IsPrivateMessage(self._dest):
      return None

    # First we need to special-case 'C++' so we can accept 'C++++' and 'C++--'.
    message = message.lower()
    pieces = message.split('c++++')
    # Loop through the list of chunks, building before-after pairs for each
    # occurrence of 'c++++' in the message.
    for i in range(1, len(pieces)):
      self._AdjustWhuffie(who, 'c++', '++',
                          ''.join(pieces[:i]), ''.join(pieces[i:]))
    pieces = message.split('c++--')
    for i in range(1, len(pieces)):
      self._AdjustWhuffie(who, 'c++', '--',
                          ''.join(pieces[:i]), ''.join(pieces[i:]))

    # Mask any 'c++++' or 'c++--' instances so the next match doesn't find
    # them.  We can't just remove them, since we need them in the reason list.
    message = message.replace('c++++', '%%plus%%').replace('c++--',
                                                           '%%minus%%')

    # finditer returns a list of match objects.
    msg = None
    matches = self.WHUFFIE_REGEXP.finditer(message)
    for match in matches:
      (target, oper) = match.groups()
      # Reinstate 'c++++' and 'c++--' in the strings before and after the
      # match.
      reason = [x.replace('%%plus%%', 'c++++').replace('%%minus%%', 'c++--')
                for x in [message[:match.start(1)], message[match.end(2):]]]
      new_msg = self._AdjustWhuffie(who, target, oper, reason[0], reason[1])
      # Save the first non-empty reply that was produced, to be returned.
      if new_msg and not msg:
        msg = new_msg
    return msg

  #
  # Internal methods
  #

  def _FindReplyMethod(self, message, who, message_list):
    """Returns the result of a matching method from the given message_list,
    or None.
    """
    for response in message_list:
      regexp = response[0]
      m = regexp.search(message)
      if m:
        method = response[1]
        return method(message, who, m)
    return None

  def _CustomHelp(self, cmd):
    """Returns the custom help reply for the given command, or None."""
    return _config['custom_help'].get(cmd, None)

  def _CustomHelpList(self):
    """Returns a list of commands for which help is available, minus 'admin'.
    """
    commands = _config['custom_help'].keys()
    if 'admin' in commands:
      commands.remove('admin')
    return commands

  def _FormatTime(self, the_time, short=False):
    """Given a datetime object, returns a formatted string.

    If short is True, the string is of the form '3/19 16:45'.
    Otherwise, it is of the form '4:45 PM on Wednesday'.
    """
    if short:
      format = '%m/%d %H:%M'
    else:
      format = '%I:%M %p on %A'
    time_str = the_time.strftime(format)
    if time_str.startswith('0'):
      time_str = time_str[1:]
    return time_str

  def _GetTime(self, zone_string):
    """Returns a string describing the current time in the given time zone.

    Args:
      zone_string: a key in the self._timezones dict (q.v.)
    """
    utc_time = datetime.datetime.now(self._timezones['utc'])
    zone = self._timezones[zone_string]
    local_time = utc_time.astimezone(zone)
    local_noon = datetime.datetime(local_time.year,
                                   local_time.month,
                                   local_time.day,
                                   hour=12, minute=0, tzinfo=zone)
    # For some reason this timedelta is 6 minutes off for Beijing (!).
    #if abs(local_time - local_noon) < datetime.timedelta(minutes=15):
    delta = ((local_time.hour * 60 + local_time.minute) -
             (local_noon.hour * 60 + local_noon.minute))
    if abs(delta) < 15:
      return 'lunchtime'
    return self._FormatTime(local_time)

  def _MakeSubstitutions(self, message, who):
    """Makes text substitutions in a reply string and returns the result.

    Known substitutions are listed below.  'who' is the user who spoke.
    """
    subs = {
      'who': who,
      'time_beijing': self._GetTime('beijing'),
      'time_denmark': self._GetTime('dk'),
      'time_tokyo': self._GetTime('jp'),
      'time_mtv': self._GetTime('mtv'),
      'time_nyc': self._GetTime('nyc'),
    }
    return message % subs

  def _Linkify(self, text):
    """Converts apparent URLs in text to HREF anchors.

    The URL parsing done here is pretty simple: if urlparse thinks a word
    has both a scheme and a netloc (i.e., a host), we linkify it.  This may
    be more costly than it could be, but it shouldn't happen often.
    """
    # Don't use the default split, or we'll lose whitespace.
    words = text.split(' ')
    for i in range(len(words)):
      word = words[i]
      parsed = urlparse.urlparse(word)
      # (scheme, netloc, path, params, query, fragment)
      if parsed[0] and parsed[1]:
        # Redirect to hide referrer.
        url = configuration.IRC.href_redirect_format % word
        words[i] = '<a href="%s">%s</a>' % (url, word)
    return ' '.join(words)

  def _AdjustWhuffie(self, who, target, oper, before, after):
    """Adds or subtracts whuffie from the target depending on the operator.

    The operator should be either '++' or '--'.
    """
    if oper == '++':
      amount = 1
    elif oper == '--':
      amount = -1
    else:
      return None

    target = target.lower()
    if target == 'me':
      target = who
    if target == who.lower() and amount > 0:
      return None

    self._whuffie_list.AdjustValue(target, amount)

    # This will save separate lines for each ++ or -- found.  That'll clutter
    # the webpage a little, but it should be uncommon, so it's good enough.
    when = self._FormatTime(datetime.datetime.now(), short=True)
    before = before or ''
    after = after or ''
    stripped = [x.strip() for x in [before, target, oper, after]]
    self._whuffie_reasons.Append('\n'.join([when] + stripped))

    if target == who.lower():  # and amount < 0
      return "There there, %s, it's not so bad." % who

    if target == self._nickname and amount > 0:
      return "Happy to help, %s." % who
    return None


  #
  # Admin methods
  #

  def _ImportData(self, message=None, who=None, match=None):
    """Read the BOT_DATA_FILE to fill the reply and action lists.  Called as
    an internal method on init and also available as an admin method.

    When evaluated as Python source, the file is expected to fill a dictionary
    named 'new_config' with all of the following items. (In the following
    discussion, a 'reply' is either a single string, or a list of strings to
    be sent in order.)  The file is aware of 'self' (i.e., this object) when
    it's evaluated, so class methods may be used.  In addition, any string in
    a reply list can start with '/me ', in which case the buildbot will attempt
    to send that as an action instead of as text; and it may contain any of
    the string substitution tags listed under _MakeSubstitutions, above.

      new_config['custom_help']: a dictionary mapping any new custom commands
          to their help replies.

      new_config['unknown_command']: a list of one or more replies, one of
          which will be chosen randomly if the user sends the buildbot a query
          it doesn't recognize (either in private chat, or in open chat with
          the bot's nickname at the start of the message).

      new_config['global_methods']: a list of tuples.  Each tuple contains a
          compiled regexp as the first item, and a method as the second. This
          list will be checked against every line received, and the method from
          the first matching line will be called.  Since a regexp search may be
          called for every item in this list for each text line received, this
          list should be kept as short as possible.

      new_config['admin_methods']: Like global_methods, but the list will only
          be checked for an ADMINISTRATOR sending a private chat.

      new_config['message_methods']: Like global_methods, but the list will
          only be checked when the user is addressing the buildbot (either in
          private chat, or in open chat with the buildbot's nickname at the
          start of the message).

      new_config['message_replies']: a list of tuples. Each tuple contains a
          compiled regexp as the first item, and a list of possible replies as
          the second.  When a user addresses the buildbot, the list will be
          checked, and a random reply from the first match will be returned.

      new_config['action_replies']: Like message_replies, but sent in response
          to user "/me" actions that appear to be interacting with the
          buildbot.

    The matches are all case-sensitive, unless the regular expressions were
    compiled with the re.IGNORECASE flag. They use re.search rather than
    re.match -- that is, they'll be found anywhere in the strings -- so they
    should include ^ and $ as needed.

    Methods accessed by these *_methods lists must accept three arguments:
        message: what was said
        who: who said it
        match: the match object resulting from the regexp search
    """
    # Find the bot data file in the module search path.
    for path in sys.path:
      file_path = os.path.join(path, Responder.BOT_DATA_FILE)
      if os.path.isfile(file_path):
        break

    error = False
    local_context = {'self': self, 'new_config': {}}
    try:
      execfile(file_path, {'re':re}, local_context)
    except IOError, e:
      if e.errno != errno.ENOENT:
        raise
      log.msg('Unable to load bot data file %s.' % file_path)
      return ("Oops! I couldn't load %s. Still using old config(?)." %
              file_path)

    # TODO(pamg): Consider consolidating the method and text-reply lists
    # using RTTI.
    global _config
    _config = local_context['new_config']
    return 'Loaded.'

  def _Disable(self, message, who, match):
    """Admin method: disables the special buildbot behavior."""
    global _bot_enabled
    if _bot_enabled:
      old = 'enabled'
    else:
      old = 'disabled'
    _bot_enabled = False
    return 'Bot now disabled (was %s)' % old

  def _Enable(self, message, who, match):
    """Admin method: (re-)enables the special buildbot behavior."""
    global _bot_enabled
    if _bot_enabled:
      old = 'enabled'
    else:
      old = 'disabled'
    _bot_enabled = True
    return 'Bot now enabled (was %s)' % old

  def _ListJokes(self, message, who, match):
    """Admin method: lists all jokes, with indexes."""
    # We need a fresh joke list in case someone on a different channel has
    # modified it since we last loaded it.
    jokes = self._joke_list.GetList(fresh=True)
    reply = []
    if len(jokes):
      for index in range(len(jokes)):
        reply.append('%d) %s' % (index, jokes[index]))
    else:
      reply = ['(No jokes in list.)']
    return reply

  def _PurgeOneJoke(self, message, who, match):
    """Admin method: completely removes one joke, by index."""
    index = int(match.group(1))
    jokes = self._joke_list.GetList(fresh=True)
    try:
      removed = jokes.pop(index)
    except IndexError:
      return 'Index out of range (0 to %d)' % (len(jokes) - 1)
    self._joke_list.Reset(jokes)
    return 'Removed joke %d (%s).' % (index, removed)

  def _PurgeJokes(self, message, who, match):
    """Admin method: permanently deletes all jokes marked for removal."""
    all_jokes = self._joke_list.GetList(fresh=True)
    jokes = [x for x in all_jokes
             if not x.startswith(Responder.JOKE_DELETED_MARKER)]
    self._joke_list.Reset(jokes)
    new_joke_count = len(jokes)
    return 'Removed %d jokes, leaving %d' % (len(all_jokes) - new_joke_count,
                                             new_joke_count)

  def _ClearAllWhuffie(self, message, who, match):
    """Admin method: resets all whuffie to zero."""
    self._whuffie_list.Reset({})
    self._whuffie_reasons.Reset([])
    return 'Cleared all whuffie.'

  def _ClearWhuffie(self, message, who, match):
    """Admin method: clears one whuffie entry."""
    target = match.group(1)
    dict = self._whuffie_list.GetDict(fresh=True)
    if target in dict:
      value = dict.pop(target)
      self._whuffie_list.Reset(dict)
      return 'Cleared whuffie for %s (had %d).' % (target, value)
    return '%s has no whuffie.' % target

  def _AdminHelp(self, message, who, match):
    """Admin method: returns admin help."""
    return self._CustomHelp('admin')


class IRCContact(words.IRCContact):
  """Subclass of the generic IRC bot that extends the reply methods."""

  # When sending more than one line in a reply, how long to wait between them.
  # Sending them at the same time doesn't maintain the order.
  REPLY_DELAY_SECS = 0.1

  def __init__(self, channel, dest, nickname):
    words.IRCContact.__init__(self, channel, dest)
    self._nickname = nickname.lower()

    # Regexp to detect when an action is directed at the buildbot.
    self._to_me_regexp = re.compile('s (with )?(the )?%s(\W|$)' %
                                    self._nickname, re.IGNORECASE)

    self._responder = Responder(dest, nickname)

  def _StartsWithNickname(self, message):
    """Returns True if the message addresses me."""
    message_lower = message.lower()
    return (message_lower.startswith("%s:" % self._nickname) or
            message_lower.startswith("%s," % self._nickname))

  def _EmitReply(self, msg_list):
    """Queues one or more strings from a reply to be spoken or acted.

    Args:
      msg_list: a string or list of strings to be sent. Any of them that begin
          with '/me ' will be sent as actions rather than chat.

    Sending an action to a private chat doesn't work properly, so those will
    be sent as text messages instead.
    """
    if not msg_list:
      return
    if isinstance(msg_list, str) or isinstance(msg_list, unicode):
      msg_list = [msg_list]
    delay = 0
    for msg in msg_list:
      # Encode to ASCII, because the IRC bot framework doesn't support
      # anything else.
      if isinstance(msg, unicode):
        msg = msg.encode('ascii', 'replace')
      action = self.send
      if msg.startswith('/me '):
        msg = msg[len('/me '):]
        # Actions with self.act don't work in private messages (?!).
        if not IsPrivateMessage(self.dest):
          action = self.act
      delay += IRCContact.REPLY_DELAY_SECS
      reactor.callLater(delay, action, msg)

  def _ValidateAndTrimNickname(self, message):
    """Makes sure the message is for us, and removes our nickname if present.
    Returns None if the message isn't for us.

    A message is "for us" if it's in private chat, or if it's in open chat
    and starts with our nickname (and appropriate punctuation).
    """
    if (not IsPrivateMessage(self.dest) and
        not self._StartsWithNickname(message)):
      return None
    if self._StartsWithNickname(message):
      message = message[len("%s:" % self._nickname):].lstrip()
    return message

  def _DecodeIncomingMessage(self, message):
    """Returns the message decoded into unicode."""
    # We don't know what encoding the incoming string is in, but we'll try
    # the three most common.  We can only emit ASCII anyway, but we want to
    # keep as much of the character data as we can for the whuffie list.
    for encoding in ['windows-1252', 'iso-8859-1', 'utf-8']:
      try:
        return message.decode(encoding)
      except UnicodeDecodeError:
        pass
    # If none of them succeeded, fall back to UTF-8 with unknown characters
    # replaced by '?'.
    return message.decode('utf-8', 'replace')

  def handleMessage(self, message, who):
    """Handles a message arriving in chat.

    Unlike the parent class, this method also expects messages that may not be
    addressed to the buildbot.
    """
    # (from words.IRCContact):
    # a message has arrived from 'who'. For broadcast contacts (i.e. when
    # people do an irc 'buildbot: command'), this will be a string
    # describing the sender of the message in some useful-to-log way, and
    # a single Contact may see messages from a variety of users. For
    # unicast contacts (i.e. when people do an irc '/msg buildbot
    # command'), a single Contact will only ever see messages from a
    # single user.

    if who.lower() == self._nickname:
      return

    global _bot_enabled
    if not _bot_enabled:
      # Re-enabling only.
      message = self._ValidateAndTrimNickname(message)
      if self.dest in ADMINISTRATORS and message == 'enable':
        self._EmitReply(self._responder._Enable(message, who, None))
      return

    message = self._DecodeIncomingMessage(message)

    # Handle methods that don't care about having our nickname at the start.
    # These are split out so we can bail early afterward if the message isn't
    # for us.
    reply = self._responder.FindReplyMethodToAnyone(message, who)
    if reply:
      self._EmitReply(reply)
      return

    message = self._ValidateAndTrimNickname(message)
    if not message:
      return

    # Handle methods that only apply to admins, in private chat.
    if self.dest in ADMINISTRATORS:
      reply = self._responder.FindReplyMethodFromAdmin(message, who)
      if reply:
        self._EmitReply(reply)
        return

    # Handle methods that only apply when our nickname is at the start.
    reply = self._responder.FindReplyMethodToMe(message, who)
    if reply:
      self._EmitReply(reply)
      return

    # Handle text replies.
    reply = self._responder.FindReply(message, who, TYPE_MESSAGE)
    if reply:
      self._EmitReply(reply)
      return

    if DEBUGGING:
      self._EmitReply('Got "%s"' % message)
    self._EmitReply(RandomChoice(self._responder.UnknownCommandReplies()))

  def handleAction(self, message, who):
    """Handles an action arriving in chat.

    Unlike the parent class, this method also expects messages that may not be
    addressed to the buildbot, so it can be more flexible in its parsing.
    """
    # (from words.IRCContact):
    # this is sent when somebody performs an action that mentions the
    # buildbot (like '/me kicks buildbot'). 'who' is the name/nick/id of
    # the person who performed the action, so if their action provokes a
    # response, they can be named.

    # TODO(pamg): For some inexplicable reason, buildbot responses to actions
    # in private chat never appear in any form.  They end up sent to the
    # handleMessage method, which no other buildbot-produced messages do,
    # but they don't show up in the channel.  If we ever care enough, we'll
    # need to fix that.

    if who.lower() == self._nickname:
      return

    global _bot_enabled
    if not _bot_enabled:
      return

    message = self._DecodeIncomingMessage(message)

    message.lstrip()
    # Remove punctuation from the end.
    while not message[-1].isalpha():
      message = message[:len(message) - 1]

    if not self._to_me_regexp.search(message):
      return

    reply = self._responder.FindReply(message, who, TYPE_ACTION)
    if reply:
      self._EmitReply(reply)

  def TopicChanged(self, user, topic):
    """Called when the topic has changed, but not when the bot starts up."""
    self._responder.TopicUpdated(user, topic)
