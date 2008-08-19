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

#ifndef CHROME_BROWSER_NAVIGATION_ENTRY_H__
#define CHROME_BROWSER_NAVIGATION_ENTRY_H__

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/security_style.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/tab_contents_type.h"
#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"

////////////////////////////////////////////////////////////////////////////////
//
// NavigationEntry class
//
// A NavigationEntry is a data structure that captures all the information
// required to recreate a browsing state. This includes some opaque binary
// state as provided by the TabContents as well as some clear text title and
// URL which is used for our user interface.
//
////////////////////////////////////////////////////////////////////////////////
class NavigationEntry {
 public:
  // Collects the SSL information for this NavigationEntry.
  class SSLStatus {
   public:
    // Flags used for the page security content status.
    enum ContentStatusFlags {
      NORMAL_CONTENT = 0,       // Neither of the 2 cases below.
      MIXED_CONTENT  = 1 << 0,  // https page containing http resources.
      UNSAFE_CONTENT = 1 << 1   // https page containing broken https resources.
    };

    SSLStatus();

    void set_security_style(SecurityStyle security_style) {
      security_style_ = security_style;
    }
    SecurityStyle security_style() const {
      return security_style_;
    }

    void set_cert_id(int ssl_cert_id) {
      cert_id_ = ssl_cert_id;
    }
    int cert_id() const {
      return cert_id_;
    }

    void set_cert_status(int ssl_cert_status) {
      cert_status_ = ssl_cert_status;
    }
    int cert_status() const {
      return cert_status_;
    }

    void set_security_bits(int security_bits) {
      security_bits_ = security_bits;
    }
    int security_bits() const {
      return security_bits_;
    }

    // Mixed content means that this page which is served over https contains
    // http sub-resources.
    void set_has_mixed_content() {
      content_status_ |= MIXED_CONTENT;
    }
    bool has_mixed_content() const {
      return (content_status_ & MIXED_CONTENT) != 0;
    }

    // Unsafe content means that this page is served over https but contains
    // https sub-resources with cert errors.
    void set_has_unsafe_content() {
      content_status_ |= UNSAFE_CONTENT;
    }
    bool has_unsafe_content() const {
      return (content_status_ & UNSAFE_CONTENT) != 0;
    }

    // Raw accessors for all the content status flags. This is used by the UI
    // tests for checking and for certain copying. Use the per-status functions
    // for normal usage.
    void set_content_status(int content_status) {
      content_status_ = content_status;
    }
    int content_status() const {
      return content_status_;
    }

   private:
    SecurityStyle security_style_;
    int cert_id_;
    int cert_status_;
    int security_bits_;
    int content_status_;  // A combination of any of the ContentStatusFlags.

    // Copy and assignment is explicitly allowed for this class.
  };

  // The type of the page an entry corresponds to.  Used by ui tests.
  enum PageType {
    NORMAL_PAGE = 0,
    ERROR_PAGE,
    INTERSTITIAL_PAGE
  };

  // Use this to get a new unique ID during construction.
  static int GetUniqueID();

  // Create a new NavigationEntry.
  explicit NavigationEntry(TabContentsType type);
  NavigationEntry(TabContentsType type,
                  SiteInstance* instance,
                  int page_id,
                  const GURL& url,
                  const std::wstring& title,
                  PageTransition::Type transition_type);

  ~NavigationEntry() {
  }

  // Return the TabContents type required to display this entry. Immutable
  // because a tab can never change its type.
  TabContentsType GetType() const { return type_; }

  // Accessors for the unique ID of this entry.  A unique ID is preserved across
  // commits and redirects, which means that sometimes a NavigationEntry's
  // unique ID needs to be set (e.g. when creating a committed entry to
  // correspond to a to-be-deleted pending entry, the pending entry's ID must be
  // copied).
  int unique_id() const { return unique_id_; }
  void set_unique_id(int unique_id) { unique_id_ = unique_id; }

  void SetSiteInstance(SiteInstance* site_instance);
  SiteInstance* site_instance() const { return site_instance_; }

  void SetURL(const GURL& url) { url_ = url; }
  const GURL& GetURL() const { return url_; }

  // All the SSL flags.
  const SSLStatus& ssl() const {
    return ssl_;
  }
  SSLStatus& ssl() {
    return ssl_;
  }

  // Set / Get the page type.
  void  SetPageType(PageType page_type) { page_type_ = page_type; }
  PageType GetPageType() const { return page_type_; }

  void SetDisplayURL(const GURL& url) {
    display_url_ = (url == url_) ? GURL() : url;
  }
  bool HasDisplayURL() const { return !display_url_.is_empty(); }
  const GURL& GetDisplayURL() const {
    return display_url_.is_empty() ? url_ : display_url_;
  }

  void SetTitle(const std::wstring& title) { title_ = title; }
  const std::wstring& GetTitle() const { return title_; }

  // WARNING: This state is saved to the database and used to restore previous
  // states. If you write a custom TabContents and provide your own state make
  // sure you have the ability to modify the format in the future while being
  // able to deal with older versions.
  void SetContentState(const std::string& state);
  const std::string& GetContentState() const { return state_; }

  void SetPageID(int page_id) { page_id_ = page_id; }
  int32 GetPageID() const { return page_id_; }

  void SetTransitionType(PageTransition::Type transition_type) {
    transition_type_ = transition_type;
  }
  PageTransition::Type GetTransitionType() const { return transition_type_; }

  // Sets the URL of the favicon.
  void SetFavIconURL(const GURL& favicon_url) { favicon_url_ = favicon_url; }

  // Returns the URL of the favicon. This may be empty if we don't know the
  // favicon, or didn't succesfully load it before navigating to another page.
  const GURL& GetFavIconURL() const { return favicon_url_; }

  // Sets the favicon for the page.
  void SetFavIcon(const SkBitmap& favicon) { favicon_ = favicon; }

  // Returns the favicon for the page. If the icon has not been explicitly set,
  // or is empty, this returns the default favicon.
  // As loading the favicon happens asynchronously, it is possible for this to
  // return the default favicon even though the page has a favicon other than
  // the default.
  const SkBitmap& GetFavIcon() const { return favicon_; }

  // Whether the favicon is valid. The favicon is valid if it represents the
  // true favicon of the site.
  void SetValidFavIcon(bool valid_fav_icon) {
    valid_fav_icon_ = valid_fav_icon;
  }
  bool IsValidFavIcon() const { return valid_fav_icon_; }

  void SetUserTypedURL(const GURL& user_typed_url) {
    user_typed_url_ = user_typed_url;
  }
  const GURL& GetUserTypedURL() const { return user_typed_url_; }

  // If the user typed url is valid it is returned, otherwise url is returned.
  const GURL& GetUserTypedURLOrURL() const {
    return user_typed_url_.is_valid() ? user_typed_url_ : url_;
  }

  bool HasPostData() const { return has_post_data_; }

  void SetHasPostData(bool has_post_data) { has_post_data_ = has_post_data; }

  // See comment above field.
  void set_restored(bool restored) { restored_ = restored; }
  bool restored() const { return restored_; }

 private:
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  // Session/Tab restore save portions of this class so that it can be recreated
  // later. If you add a new field that needs to be persisted you'll have to
  // update SessionService/TabRestoreService appropriately.
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // Unique IDs only really need to distinguish the various existing entries
  // from each other, rather than be unique over all time; so it doesn't matter
  // if this eventually wraps.
  static int unique_id_counter_;

  TabContentsType type_;

  int unique_id_;

  // If this entry is a TAB_CONTENTS_WEB, then keep a pointer to the
  // SiteInstance that it belongs to.  This allows us to reuse the same
  // process if the user goes Back across site boundaries.  If the process is
  // gone by the time the user clicks Back, a new process will be created.
  // This is NULL if this entry's type is not TAB_CONTENT_WEB.
  scoped_refptr<SiteInstance> site_instance_;

  // Describes the current page that the tab represents. This is not relevant
  // for all tab contents types.
  int32 page_id_;

  GURL url_;
  // The URL the user typed in.  May be invalid.
  GURL user_typed_url_;
  std::wstring title_;
  GURL favicon_url_;
  GURL display_url_;

  std::string state_;

  // The favorite icon for this entry.
  SkBitmap favicon_;

  PageType page_type_;

  SSLStatus ssl_;

  bool valid_fav_icon_;

  // True if this navigation needs to send post data in order to be displayed
  // properly.
  bool has_post_data_;

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  PageTransition::Type transition_type_;

  // Was this entry created from session/tab restore? If so this is true and
  // gets set to false once we navigate to it
  // (NavigationControllerBase::DidNavigateToEntry).
  bool restored_;
};

#endif  //  CHROME_BROWSER_NAVIGATION_ENTRY_H__
