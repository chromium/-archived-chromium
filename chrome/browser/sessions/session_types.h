// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_

#include <string>
#include <vector>

#include "base/gfx/rect.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/stl_util-inl.h"
#include "googleurl/src/gurl.h"

class NavigationEntry;

// TabNavigation  -------------------------------------------------------------

// TabNavigation corresponds to the parts of NavigationEntry needed to restore
// the NavigationEntry during session restore and tab restore.
//
// TabNavigation is cheap and supports copy semantics.
class TabNavigation {
 public:
  enum TypeMask {
    HAS_POST_DATA = 1
  };

  TabNavigation()
      : transition_(PageTransition::TYPED),
        type_mask_(0),
        index_(-1) {
  }

  TabNavigation(int index,
                const GURL& url,
                const GURL& referrer,
                const std::wstring& title,
                const std::string& state,
                PageTransition::Type transition)
      : url_(url),
        referrer_(referrer),
        title_(title),
        state_(state),
        transition_(transition),
        type_mask_(0),
        index_(index) {}

  // Converts this TabNavigation into a NavigationEntry with a page id of
  // |page_id|. The caller owns the returned NavigationEntry.
  NavigationEntry* ToNavigationEntry(int page_id) const;

  // Resets this TabNavigation from |entry|.
  void SetFromNavigationEntry(const NavigationEntry& entry);

  // URL of the page.
  void set_url(const GURL& url) { url_ = url; }
  const GURL& url() const { return url_; }

  // The referrer.
  const GURL& referrer() const { return referrer_; }

  // The title of the page.
  const std::wstring& title() const { return title_; }

  // State bits.
  const std::string& state() const { return state_; }

  // Transition type.
  void set_transition(PageTransition::Type transition) {
    transition_ = transition;
  }
  PageTransition::Type transition() const { return transition_; }

  // A mask used for arbitrary boolean values needed to represent a
  // NavigationEntry. Currently only contains HAS_POST_DATA or 0.
  void set_type_mask(int type_mask) { type_mask_ = type_mask; }
  int type_mask() const { return type_mask_; }

  // The index in the NavigationController. If this is -1, it means this
  // TabNavigation is bogus.
  //
  // This is used when determining the selected TabNavigation and only useful
  // by BaseSessionService and SessionService.
  void set_index(int index) { index_ = index; }
  int index() { return index_; }

 private:
  friend class BaseSessionService;

  GURL url_;
  GURL referrer_;
  std::wstring title_;
  std::string state_;
  PageTransition::Type transition_;
  int type_mask_;

  int index_;
};

// SessionTab ----------------------------------------------------------------

// SessionTab corresponds to a NavigationController.
struct SessionTab {
  SessionTab() : tab_visual_index(-1), current_navigation_index(-1) { }

  // Unique id of the window.
  SessionID window_id;

  // Unique if of the tab.
  SessionID tab_id;

  // Visual index of the tab within its window. There may be gaps in these
  // values.
  //
  // NOTE: this is really only useful for the SessionService during
  // restore, others can likely ignore this and use the order of the
  // tabs in SessionWindow.tabs.
  int tab_visual_index;

  // Identifies the index of the current navigation in navigations. For
  // example, if this is 2 it means the current navigation is navigations[2].
  //
  // NOTE: when the service is creating SessionTabs, initially this
  // corresponds to TabNavigation.index, not the index in navigations. When done
  // creating though, this is set to the index in navigations.
  int current_navigation_index;

  std::vector<TabNavigation> navigations;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionTab);
};

// SessionWindow -------------------------------------------------------------

// Describes a saved window.
struct SessionWindow {
  SessionWindow()
      : selected_tab_index(-1),
        type(Browser::TYPE_NORMAL),
        is_constrained(true),
        is_maximized(false) {}
  ~SessionWindow() { STLDeleteElements(&tabs); }

  // Identifier of the window.
  SessionID window_id;

  // Bounds of the window.
  gfx::Rect bounds;

  // Index of the selected tab in tabs; -1 if no tab is selected. After restore
  // this value is guaranteed to be a valid index into tabs.
  //
  // NOTE: when the service is creating SessionWindows, initially this
  // corresponds to SessionTab.tab_visual_index, not the index in
  // tabs. When done creating though, this is set to the index in
  // tabs.
  int selected_tab_index;

  // Type of the browser. Currently we only store browsers of type
  // TYPE_NORMAL and TYPE_POPUP.
  Browser::Type type;

  // If true, the window is constrained.
  //
  // Currently SessionService prunes all constrained windows so that session
  // restore does not attempt to restore them.
  bool is_constrained;

  // The tabs, ordered by visual order.
  std::vector<SessionTab*> tabs;

  // Is the window maximized?
  bool is_maximized;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionWindow);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
