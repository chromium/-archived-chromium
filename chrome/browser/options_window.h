// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_OPTIONS_WINDOW_H__
#define CHROME_BROWSER_OPTIONS_WINDOW_H__

class Profile;

// An identifier for a Options Tab page. These are treated as indices into
// the list of available tabs to be displayed. PAGE_DEFAULT means select the
// last tab viewed when the Options window was opened, or PAGE_GENERAL if the
// Options was never opened.
enum OptionsPage {
  OPTIONS_PAGE_DEFAULT = -1,
  OPTIONS_PAGE_GENERAL,
  OPTIONS_PAGE_CONTENT,
  OPTIONS_PAGE_ADVANCED,
  OPTIONS_PAGE_COUNT
};

// These are some well known groups within the Options dialog box that we may
// wish to highlight to attract the user's attention to.
enum OptionsGroup {
  OPTIONS_GROUP_NONE,
  OPTIONS_GROUP_DEFAULT_SEARCH
};

// Show the Options window selecting the specified page. If an Options window
// is currently open, this just activates it instead of opening a new one.
void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile);

#endif  // #ifndef CHROME_BROWSER_OPTIONS_WINDOW_H__
