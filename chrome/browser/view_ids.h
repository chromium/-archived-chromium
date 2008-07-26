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

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of a container view.

#ifndef CHROME_BROWSER_VIEW_IDS_H__
#define CHROME_BROWSER_VIEW_IDS_H__

enum ViewID {
  VIEW_ID_NONE = 0,

  // BROWSER WINDOW VIEWS
  // ------------------------------------------------------

  // Tabs within a window/tab strip, counting from the left.
  VIEW_ID_TAB_0,
  VIEW_ID_TAB_1,
  VIEW_ID_TAB_2,
  VIEW_ID_TAB_3,
  VIEW_ID_TAB_4,
  VIEW_ID_TAB_5,
  VIEW_ID_TAB_6,
  VIEW_ID_TAB_7,
  VIEW_ID_TAB_8,
  VIEW_ID_TAB_9,
  VIEW_ID_TAB_LAST,

  // Toolbar & toolbar elements.
  VIEW_ID_TOOLBAR = 1000,
  VIEW_ID_BACK_BUTTON,
  VIEW_ID_FORWARD_BUTTON,
  VIEW_ID_RELOAD_BUTTON,
  VIEW_ID_STAR_BUTTON,
  VIEW_ID_LOCATION_BAR,
  VIEW_ID_GO_BUTTON,
  VIEW_ID_PAGE_MENU,
  VIEW_ID_APP_MENU,
  VIEW_ID_AUTOCOMPLETE,

  // The Bookmark Bar.
  VIEW_ID_BOOKMARK_BAR,

  // Tab Container window.
  VIEW_ID_TAB_CONTAINER,

  VIEW_ID_PREDEFINED_COUNT
};

#endif  // CHROME_BROWSER_VIEW_IDS_H__
