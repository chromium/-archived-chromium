#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Data for use by the chatty IRC buildbot.

See the full description in irc_contact._ImportData().
"""

import re

def Compile(pattern):
  """Returns a compiled regexp combining the pattern with suitable flags."""
  return re.compile(pattern, re.IGNORECASE)

# Replies to unrecognized commands.
new_config['unknown_command'] = [
    "Huh?",
    "/me looks confused.",
    "Sorry, I don't understand that."
]

# Custom help messages.
new_config['custom_help'] = {
  'whuffie': ["<name>++ or <name>-- - give (positive or negative) whuffie.",
              "See http://www/~chrome-eng/whuffie.html",
             ],
  'admin'  : ["reload - reload the config files.",
              "list jokes - list all jokes in file, with indexes.",
              "purge jokes - purge all deleted jokes.",
              "purge joke N - purge any one joke by list index.",
              "clear all whuffie - reset everything to 0.",
              "clear whuffie NAME - reset one whuffie to 0.",
              "enable/disable - on/off switch for all the fancy stuff.",
             ],
}

# Queries matched in any broadcast message.
new_config['global_methods'] = [
  (self.WHUFFIE_REGEXP, self._FindWhuffieAdjustments),
]

# Queries matched in messages to me, resulting in calling a method.
new_config['message_methods'] = [
  (Compile('^help( [^ ]+)?'), self._GiveHelp),
  (Compile('(who|when).*topic'), self._DescribeTopic),
  (Compile('(tell|say)(?!.*(good|better)).*joke'), self._TellJoke),
  (Compile('(add|learn|listen|new).*joke: (.*)'), self._AddJoke),
  (Compile('(remove|delete) (((that |the )?last)|that) joke'),
           self._RemoveJoke),
]

# Queries matched in private messages from admins, resulting in calling a
# method.
new_config['admin_methods'] = [
  (Compile('^reload$'), self._ImportData),
  (Compile('^disable$'), self._Disable),
  (Compile('^enable$'), self._Enable),
  (Compile('^list jokes$'), self._ListJokes),
  (Compile('^purge jokes$'), self._PurgeJokes),
  (Compile('^purge joke ([\d]+)$'), self._PurgeOneJoke),
  (Compile('^clear all whuffie$'), self._ClearAllWhuffie),
  (Compile('^clear whuffie ([^ ]+)$'), self._ClearWhuffie),
  (Compile('^(admin help|help admin)$'), self._AdminHelp),
]

# Queries matched to messages to me, with lists of random replies.
new_config['message_replies'] = [
  (Compile('(tell|say).*(good|better).*joke'),
      ["Asking a bit much, aren't you?",
       "You first.",
       "Sorry, I don't know any of those."
      ],
  ),
  (Compile('(code.*review|review.*(code|patch|change))'),
      ["%(who)s: LGTM!"]
  ),
  (Compile('ping'),
      ["%(who)s: pong",
       "%(who)s: Yes?",
       "/me is alive.",
      ]
  ),
  (Compile('(^| )(a/?s/?l|age/sex/location)(\W|$)'),
      ['9 months/N/Mountain View',
       'ageless/sexless/teh intarwebs',
       'Whatever you want me to be. :)',
      ]
  ),
  (Compile("(what( is|'s)|tell .*) ?the meaning of life"),
      ["Build and test.",
       "The goal of existence is a green tree.",
       "It's a movie by Monty Python: http://www.imdb.com/title/tt0085959/",
       "42. No, sorry, I was thinking of something else.",
      ]
  ),
  (Compile('who (are you|you are)'),
      ["I'm buildbot!",
       'Only the shallow know themselves.',  # Oscar Wilde
       'The final mystery is oneself.',  # Oscar Wilde
       "One's only real life is the life one never leads.",  # Oscar Wilde
       'There is as much difference between us and ourselves as between'
           ' us and others.',  # Michel de Montaigne
       'The words "I am" are potent words; be careful what you hitch them to.',
           # A. L. Kitselman
      ]
  ),
  # These codes (for upper- and lower-case A with ring) will generally only
  # work if the user is using the windows-1252 encoding. Otherwise, since we
  # try windows-1252 first, we may erroneously decode it as a different
  # sequence of characters.  In the case of utf-8, at least, for some reason
  # we're unable to match that other sequence of characters properly.
  # TODO(pamg): If anybody ever notices and cares, look into this more.
  (Compile(u'time.*(denmark|(aa?|\xc5|\xe5)rhus)'),
      ["It's %(time_denmark)s in Aarhus."]
  ),
  (Compile('time.*(delft|netherlands|holland)'),
      ["It's %(time_denmark)s in Delft."]
  ),
  (Compile('time.*(beijing|china)'),
      ["It's %(time_beijing)s in Beijing."]
  ),
  (Compile('time.*(tokyo|japan)'),
      ["It's %(time_tokyo)s in Tokyo."]
  ),
  (Compile('time.*(ny|nyc|new york)'),
      ["It's %(time_nyc)s in NYC."]
  ),
  # See note by the Aarhus entry, above, about character encodings.
  (Compile(u'time.*qu(e|\xe9)bec'),
      ["It's %(time_nyc)s in Quebec City."]
  ),
  (Compile(u'time.*montr(e|\xe9)al'),
      ["It's %(time_nyc)s in Montreal."]
  ),
  (Compile('time.*(virginia|va|reston)'),
      ["It's %(time_nyc)s in Reston, VA."]
  ),
  (Compile('time.*(kirkland|wa|washington)'),
      ["It's %(time_mtv)s in Kirkland."]
  ),
  (Compile('time.*(mtv|mountain view|california|ca|is it[\?\.\!]|is it$)'),
      ["It's %(time_mtv)s in Mountain View."]
  ),
  (Compile('time[\?\.\!]?$'),  # for "what is the time?"
      ["It's %(time_mtv)s in Mountain View."]
  ),
  (Compile('time'),
      ["Sorry, I'm not sure where you're asking about. "
           "It's %(time_mtv)s in Mountain View, if that's any help."]
  ),
  (Compile('what.*whuffie'),
      ["Whuffie is reputation currency, a sort of \"virtual karma\", based"
       " on Cory Doctorow's novel _Down and Out in the Magic Kingdom_."
       " Try 'help whuffie' for related commands."]
  ),
]

# Queries matched to actions directed at me, with lists of random replies.
new_config['action_replies'] = [
  (Compile('(kicks|hits|punches|slaps)'),
      ["Hey!",
       "Ow!",
       "/me kicks %(who)s back.",
       "/me pouts."
      ],
  ),
  (Compile('pokes'),
      ["Hey!",
       "/me pokes %(who)s back.",
      ],
  ),
  (Compile('dances.*with'),
      ["/me dances!",
       "/me does the lambada with %(who)s!",
       "/me starts clogging madly.",
       "/me does the hokey pokey with %(who)s.",
       "/me jitterbugs with %(who)s.",
       "/me does the mashed potato with %(who)s.",
       "/me pirouettes around %(who)s.",
       "/me cranks out a killer breakdance. It's on fire!",
       "/me starts a tango. Not the GUI, the dance.",
       "/me kicks up its heels."
      ]
  ),
  (Compile('(hugs|kisses)'),
      ["/me hugs %(who)s back.",
       "/me blushes.",
       "Aww, shucks.",
      ]
  ),
]
