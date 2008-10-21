#!/usr/bin/python
# Copyright (c) 2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Source file for chromium_status testcases."""

import pmock
import unittest
import chromium_status

class GoogleStatusTests(unittest.TestCase):

  def testBuilderStatusByDefaultIsUnkowns(self):
    irc_topic_watcher = self._CreateIrcTopicWatcher('webkit-vyym',
                                                    'webkit-zzzz')
    for builder in irc_topic_watcher.status.botmaster.builders.values():
      self.assertEqual(chromium_status.UNKNOWN, builder.tree_status)

  def testReflectTopicNameOnBuilders1(self):
    name_status_mapping = {
                            'webkit-vyym': chromium_status.CLOSED,
                            'webkit-zzzz': chromium_status.CLOSED,
                            'ruby-rocks': chromium_status.OPEN
                          }
    irc_topic_watcher = self._CreateIrcTopicWatcher(
        *name_status_mapping.keys())

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'webkit is closed and rest are open', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'everything is open except webkit', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'webkit tree is closed', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    # Case is a bit weird and will really complicate our logic and
    # implementing does not provide too much value.
    # irc_topic_watcher._ReflectTopicNameOnBuilders(
    #     'user', 'channel', 'everything is closed except ruby')
    # self._AssertStatus(irc_topic_watcher, name_status_mapping)


  def testReflectTopicNameOnBuilders2(self):
    name_status_mapping = {
                            'webkit-vyym': chromium_status.CLOSED,
                            'webkit-zzzz': chromium_status.CLOSED,
                            'ruby-rocks': chromium_status.CLOSED
                          }

    irc_topic_watcher = self._CreateIrcTopicWatcher(
        *name_status_mapping.keys())

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'webkit, ruby are closed; rest are open', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders('everything is closed',
                                                  'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders('all trees are closed',
                                                  'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders('tree is closed', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'everything is open except webkit and ruby', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'everything is open except webkit tree and ruby tree', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)


  def testReflectTopicNameOnBuilders3(self):
    name_status_mapping = {
                            'webkit-vyym': chromium_status.OPEN,
                            'webkit-zzzz': chromium_status.OPEN,
                            'ruby-rocks': chromium_status.OPEN
                          }
    irc_topic_watcher = self._CreateIrcTopicWatcher(
        *name_status_mapping.keys())

    irc_topic_watcher._ReflectTopicNameOnBuilders('everything is cool',
                                                  'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'webkit and python are open', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

  def testReflectTopicNameOnBuilders4(self):

    topic = """chrome is
    closed because of test failures, and webkit is closed due to a flying
    armadillo attack. """
    name_status_mapping = {
                            'webkit-vyym': chromium_status.CLOSED,
                            'chrome-snappy': chromium_status.CLOSED,
                            'V8-release': chromium_status.OPEN
                          }
    irc_topic_watcher = self._CreateIrcTopicWatcher(
        *name_status_mapping.keys())

    irc_topic_watcher._ReflectTopicNameOnBuilders(topic, 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)

  def testReflectTopicNameOnBuildersWhenProjectsAreSeparatedByComma(self):
    name_status_mapping = {
                            'webkit-vyym': chromium_status.CLOSED,
                            'webkit-zzzz': chromium_status.CLOSED,
                            'ruby-rocks': chromium_status.CLOSED
                          }

    irc_topic_watcher = self._CreateIrcTopicWatcher(
        *name_status_mapping.keys())

    irc_topic_watcher._ReflectTopicNameOnBuilders(
        'webkit, ruby are closed; rest are open.', 'channel')
    self._AssertStatus(irc_topic_watcher, name_status_mapping)


  def _CreateStatus(self, *buildernames):
    status = pmock.Mock()
    botmaster = pmock.Mock()
    status.botmaster = botmaster
    status.botmaster.builders = {} # a {name: builder} hash

    for buildername in buildernames:
      builder = pmock.Mock()
      botmaster.builders[buildername] = builder
    return status

  def _CreateIrcTopicWatcher(self, *buildernames):
    status = self._CreateStatus(*buildernames)
    irc_topic_watcher = chromium_status.IrcStatusBot(
        'nickname', 'password', 'channels', status, 'categories')
    return irc_topic_watcher

  def _AssertStatus(self, irc_topic_watcher, name_status_mapping):
    for buildername in irc_topic_watcher.status.botmaster.builders:
      builder = irc_topic_watcher.status.botmaster.builders[buildername]
      self.assertEqual(
          name_status_mapping[buildername], builder.tree_status,
          'Expected  a %s builder to have a status %s but was %s'
          % (buildername, name_status_mapping[buildername],
              builder.tree_status))

if __name__ == '__main__':
  unittest.main()
