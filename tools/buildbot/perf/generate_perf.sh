#!/bin/sh
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Initializes all the perf directories.
# TODO(nsylvain): Convert to python.

# Create or reinitialize a standard perf test directory.
function InitDir()
{
  # Create the directory if it does not exist.
  mkdir -p $1

  # remove the current files, if any.
  rm -f $1/details.html
  rm -f $1/report.html
  rm -f $1/js
}

# Util function to create config.js file.
# The first parameter is the test directory.
# The second parameter is the test title.
function WriteConfig()
{
  echo "var Config = {" > $1/config.js
  echo "  title: '$2'," >>  $1/config.js
  echo "  changeLinkPrefix: 'http://src.chromium.org/viewvc/chrome?view=rev&revision='" >> $1/config.js
  echo "};" >> $1/config.js
}

# Initialize the dashboard for a ui perf test.
# The first parameter is the test directory.
# The second parameter is the name of the test.
function InitUIDashboard()
{
  # Create or reinitialize the directory.
  InitDir $1

  # Create the new file
  ln -s ../../dashboard/ui/details.html $1/
  ln -s ../../dashboard/ui/generic_plotter.html $1/report.html
  ln -s ../../dashboard/ui/js $1/

  # Create the config.js file.
  WriteConfig $1 "$2"
}

# Initialize all the perf directories for a tester.
# The first parameter is the root directory for the tester.
function InitTester()
{
  mkdir -p $1

  # Add any new perf tests here
  InitUIDashboard $1/moz "Page Cycler Moz"
  InitUIDashboard $1/intl1 "Page Cycler Intl1"
  InitUIDashboard $1/intl2 "Page Cycler Intl2"
  InitUIDashboard $1/dhtml "Page Cycler DHTML"
  InitUIDashboard $1/moz-http "Page Cycler Moz - HTTP"
  InitUIDashboard $1/new-tab-ui-cold "New Tab Cold"
  InitUIDashboard $1/new-tab-ui-warm "New Tab Warm"
  InitUIDashboard $1/startup "Startup"
  InitUIDashboard $1/tab-switching "Tab Switching"
  InitUIDashboard $1/bloat-http "Bloat - HTTP"

  InitUIDashboard $1/memory "Memory"
  
  chmod -R 755 $1
}

# Add any new perf testers here.
InitTester xp-release-dual-core
InitTester xp-release-jsc
InitTester vista-release-dual-core

