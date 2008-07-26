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

#ifndef CHROME_BROWSER_TAB_CONTENTS_TYPE_H__
#define CHROME_BROWSER_TAB_CONTENTS_TYPE_H__

// The different kinds of tab contents we support. This is declared outside of
// TabContents to eliminate the circular dependency between NavigationEntry
// (which requires a tab type) and TabContents (which requires a
// NavigationEntry).
enum TabContentsType {
  TAB_CONTENTS_UNKNOWN_TYPE = 0,
  TAB_CONTENTS_WEB,
  TAB_CONTENTS_DOWNLOAD_VIEW,
  TAB_CONTENTS_NETWORK_STATUS_VIEW,
  TAB_CONTENTS_IPC_STATUS_VIEW,
  TAB_CONTENTS_CHROME_VIEW_CONTENTS,
  TAB_CONTENTS_NEW_TAB_UI,
  TAB_CONTENTS_NATIVE_UI,
  TAB_CONTENTS_ABOUT_INTERNETS_STATUS_VIEW,
  TAB_CONTENTS_VIEW_SOURCE,
  TAB_CONTENTS_HTML_DIALOG,
  TAB_CONTENTS_ABOUT_UI,
  TAB_CONTENTS_NUM_TYPES
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TYPE_H__
