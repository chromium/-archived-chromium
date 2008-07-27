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

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEMP_NAVIGATION_ENTRY_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEMP_NAVIGATION_ENTRY_H__

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/tools/test_shell/temp/page_transition_types.h"
#include "googleurl/src/gurl.h"

////////////////////////////////////////////////////////////////////////////////
//
// NavigationEntry class
//
// A NavigationEntry is a data structure that captures all the information
// required to recreate a browsing state. This includes some opaque binary
// state as provided by the TabContents as well as some clear text title and
// uri which is used for our user interface.
//
////////////////////////////////////////////////////////////////////////////////
class NavigationEntry {
 public:
  // Create a new NavigationEntry.
  explicit NavigationEntry(TabContentsType type)
      : type_(type),
        page_id_(-1),
        transition_(PageTransition::LINK) {
  }

  NavigationEntry(TabContentsType type,
                  int page_id,
                  const GURL& url,
                  const std::wstring& title,
                  PageTransition::Type transition)
      : type_(type),
        page_id_(page_id),
        url_(url),
        title_(title),
        transition_(transition) {
  }

  // virtual to allow test_shell to extend the class.
  virtual ~NavigationEntry() {
  }

  // Return the TabContents type required to display this entry. Immutable
  // because a tab can never change its type.
  TabContentsType GetType() const { return type_; }

  // Set / Get the URI
  void SetURL(const GURL& url) { url_ = url; }
  const GURL& GetURL() const { return url_; }

  void SetDisplayURL(const GURL& url) {
    if (url == url_) {
      display_url_ = GURL::EmptyGURL();
    } else {
      display_url_ = url;
    }
  }

  bool HasDisplayURL() { return !display_url_.is_empty(); }

  const GURL& GetDisplayURL() const {
    if (display_url_.is_empty()) {
      return url_;
    } else {
      return display_url_;
    }
  }

  // Set / Get the title
  void SetTitle(const std::wstring& a_title) { title_ = a_title; }
  const std::wstring& GetTitle() const { return title_; }

  // Set / Get opaque state.
  // WARNING: This state is saved to the database and used to restore previous
  // states. If you use write a custom TabContents and provide your own
  // state make sure you have the ability to modify the format in the future
  // while being able to deal with older versions.
  void SetContentState(const std::string& state) { state_ = state; }
  const std::string& GetContentState() const { return state_; }

  // Get the page id corresponding to the tab's state.
  void SetPageID(int page_id) { page_id_ = page_id; }
  int32 GetPageID() const { return page_id_; }

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  void SetTransition(PageTransition::Type transition) {
    transition_ = transition;
  }
  PageTransition::Type GetTransition() const { return transition_; }

  // Set / Get favicon URL.
  void SetFavIconURL(const GURL& url) { fav_icon_url_ = url; }
  const GURL& GetFavIconURL() const { return fav_icon_url_; }

  // This is the URL the user typed in. This may be invalid.
  void SetUserTypedURL(const GURL& url) { user_typed_url_ = url; }
  const GURL& GetUserTypedURL() const { return user_typed_url_; }

  // If the user typed url is valid it is returned, otherwise url is returned.
  const GURL& GetUserTypedURLOrURL() const {
    return user_typed_url_.is_valid() ? user_typed_url_ : url_;
  }

 private:
  TabContentsType type_;

  // Describes the current page that the tab represents. This is not relevant
  // for all tab contents types.
  int32 page_id_;

  GURL url_;
  // The URL the user typed in.
  GURL user_typed_url_;
  std::wstring title_;
  GURL fav_icon_url_;
  GURL display_url_;

  std::string state_;

  PageTransition::Type transition_;
};

#endif  //  WEBKIT_TOOLS_TEST_SHELL_TEMP_NAVIGATION_ENTRY_H__
