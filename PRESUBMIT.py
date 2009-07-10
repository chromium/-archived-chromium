#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

EXCLUDED_PATHS = (
    r"breakpad[\\\/].*",
    r"skia[\\\/].*",
    r"v8[\\\/].*",
)


def CheckChangeOnUpload(input_api, output_api):
  results = []
  # What does this code do?
  # It loads the default black list (e.g. third_party, experimental, etc) and
  # add our black list (breakpad, skia and v8 are still not following
  # google style and are not really living this repository).
  # See presubmit_support.py InputApi.FilterSourceFile for the (simple) usage.
  black_list = input_api.DEFAULT_BLACK_LIST + EXCLUDED_PATHS
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  black_list = input_api.DEFAULT_BLACK_LIST + EXCLUDED_PATHS
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, sources))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  # Make sure the tree is 'open'.
  # TODO(maruel): Run it in a separate thread to parallelize checks?
  results.extend(CheckTreeIsOpen(input_api, output_api,
                                 'http://chromium-status.appspot.com/status',
                                 '0',
                                 'http://chromium-status.appspot.com/current'))
  results.extend(CheckTryJobExecution(input_api, output_api))
  return results


def CheckTryJobExecution(input_api, output_api):
  outputs = []
  if not input_api.change.issue or not input_api.change.patchset:
    return outputs
  url = "http://codereview.chromium.org/%d/get_build_results/%d" % (
            input_api.change.issue, input_api.change.patchset)
  try:
    connection = input_api.urllib2.urlopen(url)
    # platform|status|url
    values = [item.split('|', 2) for item in connection.read().splitlines()]
    connection.close()
    statuses = map(lambda x: x[1], values)
    message = None
    if 'failure' in statuses:
      failures = filter(lambda x: x[1] != 'success', values)
      long_text = '\n'.join("% 5s: % 7s %s" % (item[0], item[1], item[2])
                            for item in failures)
      message = 'You had try job failures. Are you sure you want to check-in?\n'
    elif 'pending' in statuses or len(values) != 3:
      long_text = '\n'.join("% 5s: % 7s %s" % (item[0], item[1], item[2])
                            for item in values)
      message = 'You should try the patch first (and wait for it to finish).\n'
    if message:
      message += (
          'Is try server wrong or broken? Please notify maruel@chromium.org. '
          'Thanks.\n')
      outputs.append(output_api.PresubmitPromptWarning(message=message,
                                                       long_text=long_text))
  except input_api.urllib2.HTTPError, e:
    if e.code == 404:
      # Fallback to no try job.
      # TODO(maruel): Change to a PresubmitPromptWarning once the try server is
      # stable enough and it seems to work fine.
      outputs.append(output_api.PresubmitNotifyResult(
          'You should try the patch first.'))
    else:
      # Another HTTP error happened, warn the user.
      # TODO(maruel): Change to a PresubmitPromptWarning once it deemed to work
      # fine.
      outputs.append(output_api.PresubmitNotifyResult(
          'Got %s while looking for try job status.' % str(e)))
  return outputs


def CheckTreeIsOpen(input_api, output_api, url, closed, url_text):
  """Similar to the one in presubmit_canned_checks except it shows an helpful
  status text instead.
  """
  assert(input_api.is_committing)
  try:
    connection = input_api.urllib2.urlopen(url)
    status = connection.read()
    connection.close()
    if input_api.re.match(closed, status):
      long_text = status + '\n' + url
      try:
        connection = input_api.urllib2.urlopen(url_text)
        text = connection.read()
        connection.close()
        match = input_api.re.search(r"\<div class\=\"Notice\"\>(.*)\<\/div\>",
                                    text)
        if match:
          long_text = match.group(1).strip()
      except IOError:
        pass
      return [output_api.PresubmitPromptWarning("The tree is closed.",
                                                long_text=long_text)]
  except IOError:
    pass
  return []
