// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_TYPES_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_TYPES_H_

#include <gtk/gtk.h>
#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/singleton.h"

typedef unsigned long Atom;

// TODO(sky): move and rename.
class TabOverviewTypes {
 public:
  enum AtomType {
    ATOM_CHROME_WINDOW_TYPE = 0,
    ATOM_CHROME_WM_MESSAGE,
    ATOM_MANAGER,
    ATOM_NET_SUPPORTING_WM_CHECK,
    ATOM_NET_WM_NAME,
    ATOM_PRIMARY,
    ATOM_STRING,
    ATOM_UTF8_STRING,
    ATOM_WM_NORMAL_HINTS,
    ATOM_WM_S0,
    ATOM_WM_STATE,
    ATOM_WM_TRANSIENT_FOR,
    kNumAtoms,
  };

  enum WindowType {
    WINDOW_TYPE_UNKNOWN = 0,

    // A top-level Chrome window.
    WINDOW_TYPE_CHROME_TOPLEVEL,

    // A window showing scaled-down views of all of the tabs within a
    // Chrome window.
    WINDOW_TYPE_CHROME_TAB_SUMMARY,

    // A tab that's been detached from a Chrome window and is currently
    // being dragged.
    //   param[0]: Cursor's initial X position at the start of the drag
    //   param[1]: Cursor's initial Y position
    //   param[2]: X component of cursor's offset from upper-left corner of
    //             tab at start of drag
    //   param[3]: Y component of cursor's offset
    WINDOW_TYPE_CHROME_FLOATING_TAB,

    kNumWindowTypes,
  };

  struct Message {
   public:
    enum Type {
      UNKNOWN = 0,

      // Instruct a top-level Chrome window to change the visibility of its
      // tab summary window.
      //   param[0]: desired visibility (0 means hide, 1 means show)
      CHROME_SET_TAB_SUMMARY_VISIBILITY,

      // Instruct the WM to move a floating tab.  The passed-in position is
      // that of the cursor; the tab's composited window is displaced based
      // on the cursor's offset from the upper-left corner of the tab at
      // the start of the drag.
      //   param[0]: X ID of the floating tab window
      //   param[1]: X coordinate to which the tab should be moved
      //   param[2]: Y coordinate
      WM_MOVE_FLOATING_TAB,

      // Notify Chrome when a floating tab has entered or left a top-level
      // window.  Sent to the window being entered/left.
      //   param[0]: X ID of the floating tab window
      //   param[1]: state (0 means left, 1 means entered)
      CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL,

      // Notify Chrome when a floating tab has entered or left a tab
      // summary window.  Sent to the summary window.
      //   param[0]: X ID of the floating tab window
      //   param[1]: state (0 means left, 1 means entered or currently in)
      //   param[2]: X coordinate relative to summary window
      //   param[3]: Y coordinate
      CHROME_NOTIFY_FLOATING_TAB_OVER_TAB_SUMMARY,

      kNumTypes,
    };

    Message() {
      Init(UNKNOWN);
    }
    Message(Type type) {
      Init(type);
    }

    Type type() const { return type_; }
    void set_type(Type type) { type_ = type; }

    inline int max_params() const {
      return arraysize(params_);
    }
    long param(int index) const {
      DCHECK_GE(index, 0);
      DCHECK_LT(index, max_params());
      return params_[index];
    }
    void set_param(int index, long value) {
      DCHECK_GE(index, 0);
      DCHECK_LT(index, max_params());
      params_[index] = value;
    }

   private:
    // Common initialization code shared between constructors.
    void Init(Type type) {
      set_type(type);
      for (int i = 0; i < max_params(); ++i) {
        set_param(i, 0);
      }
    }

    // Type of message that was sent.
    Type type_;

    // Type-specific data.  This is bounded by the number of 32-bit values
    // that we can pack into a ClientMessageEvent -- it holds five, but we
    // use the first one to store the message type.
    long params_[4];
  };

  // Returns the single instance of TabOverviewTypes.
  static TabOverviewTypes* instance();

  // Get or set a property describing a window's type.  Type-specific
  // parameters may also be supplied ('params' is mandatory for
  // GetWindowType() but optional for SetWindowType()).  The caller is
  // responsible for trapping errors from the X server.
  // TODO: Trap these ourselves.
  bool SetWindowType(GtkWidget* widget,
                     WindowType type,
                     const std::vector<int>* params);

  // If |event| is a valid Message it is decoded into |msg| and true is
  // returned. If false is returned, |event| is not a valid Message.
  bool DecodeMessage(const GdkEventClient& event, Message* msg);

 private:
  friend struct DefaultSingletonTraits<TabOverviewTypes>;

  TabOverviewTypes();

  // Maps from our Atom enum to the X server's atom IDs and from the
  // server's IDs to atoms' string names.  These maps aren't necessarily in
  // sync; 'atom_to_xatom_' is constant after the constructor finishes but
  // GetName() caches additional string mappings in 'xatom_to_string_'.
  std::map<AtomType, Atom> type_to_atom_;
  std::map<Atom, std::string> atom_to_string_;

  // Cached value of type_to_atom_[ATOM_CHROME_WM_MESSAGE].
  Atom wm_message_atom_;

  DISALLOW_COPY_AND_ASSIGN(TabOverviewTypes);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_OVERVIEW_TYPES_H_
