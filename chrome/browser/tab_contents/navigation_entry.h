// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_ENTRY_H_
#define CHROME_BROWSER_NAVIGATION_ENTRY_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/tab_contents/security_style.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"
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
  // SSL -----------------------------------------------------------------------

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

    // Raw accessors for all the content status flags. This contains a
    // combination of any of the ContentStatusFlags defined above. It is used
    // by the UI tests for checking and for certain copying. Use the per-status
    // functions for normal usage.
    void set_content_status(int content_status) {
      content_status_ = content_status;
    }
    int content_status() const {
      return content_status_;
    }

   private:
    // See the accessors above for descriptions.
    SecurityStyle security_style_;
    int cert_id_;
    int cert_status_;
    int security_bits_;
    int content_status_;

    // Copy and assignment is explicitly allowed for this class.
  };

  // The type of the page an entry corresponds to.  Used by ui tests.
  enum PageType {
    NORMAL_PAGE = 0,
    ERROR_PAGE,
    INTERSTITIAL_PAGE
  };

  // Favicon -------------------------------------------------------------------

  // Collects the favicon related information for a NavigationEntry.
  class FaviconStatus {
   public:
    FaviconStatus();

    // Indicates whether we've gotten an official favicon for the page, or are
    // just using the default favicon.
    void set_is_valid(bool is_valid) {
      valid_ = is_valid;
    }
    bool is_valid() const {
      return valid_;
    }

    // The URL of the favicon which was used to load it off the web.
    void set_url(const GURL& favicon_url) {
      url_ = favicon_url;
    }
    const GURL& url() const {
      return url_;
    }

    // The favicon bitmap for the page. If the favicon has not been explicitly
    // set or it empty, it will return the default favicon. Note that this is
    // loaded asynchronously, so even if the favicon URL is valid we may return
    // the default favicon if we haven't gotten the data yet.
    void set_bitmap(const SkBitmap& bitmap) {
      bitmap_ = bitmap;
    }
    const SkBitmap& bitmap() const {
      return bitmap_;
    }

   private:
    // See the accessors above for descriptions.
    bool valid_;
    GURL url_;
    SkBitmap bitmap_;

    // Copy and assignment is explicitly allowed for this class.
  };

  // ---------------------------------------------------------------------------

  explicit NavigationEntry(TabContentsType type);
  NavigationEntry(TabContentsType type,
                  SiteInstance* instance,
                  int page_id,
                  const GURL& url,
                  const GURL& referrer,
                  const std::wstring& title,
                  PageTransition::Type transition_type);
  ~NavigationEntry() {
  }

  // Page-related stuff --------------------------------------------------------

  // A unique ID is preserved across commits and redirects, which means that
  // sometimes a NavigationEntry's unique ID needs to be set (e.g. when
  // creating a committed entry to correspond to a to-be-deleted pending entry,
  // the pending entry's ID must be copied).
  void set_unique_id(int unique_id) {
    unique_id_ = unique_id;
  }
  int unique_id() const {
    return unique_id_;
  }

  // Return the TabContents type required to display this entry. Immutable
  // because a tab can never change its type.
  TabContentsType tab_type() const {
    return tab_type_;
  }

  // The SiteInstance tells us how to share sub-processes when the tab type is
  // TAB_CONTENTS_WEB. This will be NULL otherwise. This is a reference counted
  // pointer to a shared site instance.
  //
  // Note that the SiteInstance should usually not be changed after it is set,
  // but this may happen if the NavigationEntry was cloned and needs to use a
  // different SiteInstance.
  void set_site_instance(SiteInstance* site_instance) {
    site_instance_ = site_instance;
  }
  SiteInstance* site_instance() const {
    return site_instance_;
  }

  // The page type tells us if this entry is for an interstitial or error page.
  // See the PageType enum above.
  void set_page_type(PageType page_type) {
    page_type_ = page_type;
  }
  PageType page_type() const {
    return page_type_;
  }

  // The actual URL of the page. For some about pages, this may be a scary
  // data: URL or something like that. Use display_url() below for showing to
  // the user.
  void set_url(const GURL& url) {
    url_ = url;
    if (display_url_.is_empty()) {
      // If there is no explicit display URL, then we'll display this URL.
      display_url_as_string_ = UTF8ToWide(url_.spec());
    }
  }
  const GURL& url() const {
    return url_;
  }

  // The referring URL. Can be empty.
  void set_referrer(const GURL& referrer) {
    referrer_ = referrer;
  }
  const GURL& referrer() const {
    return referrer_;
  }

  // The display URL, when nonempty, will override the actual URL of the page
  // when we display it to the user. This allows us to have nice and friendly
  // URLs that the user sees for things like about: URLs, but actually feed
  // the renderer a data URL that results in the content loading.
  //
  // display_url() will return the URL to display to the user in all cases, so
  // if there is no overridden display URL, it will return the actual one.
  void set_display_url(const GURL& url) {
    display_url_ = (url == url_) ? GURL() : url;
    display_url_as_string_ = UTF8ToWide(url.spec());
  }
  bool has_display_url() const {
    return !display_url_.is_empty();
  }
  const GURL& display_url() const {
    return display_url_.is_empty() ? url_ : display_url_;
  }

  // The title as set by the page. This will be empty if there is no title set.
  // The caller is responsible for detecting when there is no title and
  // displaying the appropriate "Untitled" label if this is being displayed to
  // the user.
  void set_title(const std::wstring& title) {
    title_ = title;
  }
  const std::wstring& title() const {
    return title_;
  }

  // The favicon data and tracking information. See FaviconStatus above.
  const FaviconStatus& favicon() const {
    return favicon_;
  }
  FaviconStatus& favicon() {
    return favicon_;
  }

  // Content state is an opaque blob created by WebKit that represents the
  // state of the page. This includes form entries and scroll position for each
  // frame. We store it so that we can supply it back to WebKit to restore form
  // state properly when the user goes back and forward.
  //
  // WARNING: This state is saved to the file and used to restore previous
  // states. If you write a custom TabContents and provide your own state make
  // sure you have the ability to modify the format in the future while being
  // able to deal with older versions.
  void set_content_state(const std::string& state) {
    content_state_ = state;
  }
  const std::string& content_state() const {
    return content_state_;
  }

  // Describes the current page that the tab represents. For web pages
  // (TAB_CONTENTS_WEB) this is the ID that the renderer generated for the page
  // and is how we can tell new versus renavigations.
  void set_page_id(int page_id) {
    page_id_ = page_id;
  }
  int32 page_id() const {
    return page_id_;
  }

  // All the SSL flags and state. See SSLStatus above.
  const SSLStatus& ssl() const {
    return ssl_;
  }
  SSLStatus& ssl() {
    return ssl_;
  }

  // Tracking stuff ------------------------------------------------------------

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  void set_transition_type(PageTransition::Type transition_type) {
    transition_type_ = transition_type;
  }
  PageTransition::Type transition_type() const {
    return transition_type_;
  }

  // The user typed URL was the URL that the user initiated the navigation
  // with, regardless of any redirects. This is used to generate keywords, for
  // example, based on "what the user thinks the site is called" rather than
  // what it's actually called. For example, if the user types "foo.com", that
  // may redirect somewhere arbitrary like "bar.com/foo", and we want to use
  // the name that the user things of the site as having.
  //
  // This URL will be is_empty() if the URL was navigated to some other way.
  // Callers should fall back on using the regular or display URL in this case.
  void set_user_typed_url(const GURL& user_typed_url) {
    user_typed_url_ = user_typed_url;
  }
  const GURL& user_typed_url() const {
    return user_typed_url_;
  }

  // Post data is form data that was posted to get to this page. The data will
  // have to be reposted to reload the page properly. This flag indicates
  // whether the page had post data.
  //
  // The actual post data is stored in the content_state and is extracted by
  // WebKit to actually make the request.
  void set_has_post_data(bool has_post_data) {
    has_post_data_ = has_post_data;
  }
  bool has_post_data() const {
    return has_post_data_;
  }

  // Was this entry created from session/tab restore? If so this is true and
  // gets set to false once we navigate to it.
  // (See NavigationController::DidNavigateToEntry).
  void set_restored(bool restored) {
    restored_ = restored;
  }
  bool restored() const {
    return restored_;
  }

  // Returns the title to be displayed on the tab. This could be the title of
  // the page if it is available or the URL.
  const std::wstring& GetTitleForDisplay();

 private:
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  // Session/Tab restore save portions of this class so that it can be recreated
  // later. If you add a new field that needs to be persisted you'll have to
  // update SessionService/TabRestoreService appropriately.
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // See the accessors above for descriptions.
  int unique_id_;
  TabContentsType tab_type_;
  scoped_refptr<SiteInstance> site_instance_;
  PageType page_type_;
  GURL url_;
  GURL referrer_;

  GURL display_url_;

  // We cache a copy of the display URL as a string so we don't have to
  // convert the display URL to a wide string every time we paint.
  std::wstring display_url_as_string_;

  std::wstring title_;
  FaviconStatus favicon_;
  std::string content_state_;
  int32 page_id_;
  SSLStatus ssl_;
  PageTransition::Type transition_type_;
  GURL user_typed_url_;
  bool has_post_data_;
  bool restored_;

  // Copy and assignment is explicitly allowed for this class.
};

#endif  // CHROME_BROWSER_NAVIGATION_ENTRY_H_
