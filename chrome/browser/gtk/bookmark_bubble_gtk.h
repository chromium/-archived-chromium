// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the bookmark bubble, the dialog box
// presented to create or edit a bookmark.  There can only ever be a single
// bubble open, so the class presents only static methods, and handles the
// singleton behavior for you.  It also handles the object and widget
// lifetimes, destroying everything and possibly committing any changes when
// the bubble is closed.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "googleurl/src/gurl.h"

class Profile;
namespace gfx {
class Rect;
}

class BookmarkBubbleGtk : public InfoBubbleGtkDelegate {
 public:
  // Shows the bookmark bubble, pointing at |rect|.
  static void Show(GtkWindow* transient_toplevel,
                   const gfx::Rect& rect,
                   Profile* profile,
                   const GURL& url,
                   bool newly_bookmarked);

  // Implements the InfoBubbleGtkDelegate.  We are notified when the bubble
  // is about to be closed, so we have a chance to save any state / input in
  // our widgets before they are destroyed.
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

 private:
  BookmarkBubbleGtk(GtkWindow* transient_toplevel,
                    const gfx::Rect& rect,
                    Profile* profile,
                    const GURL& url,
                    bool newly_bookmarked);
  ~BookmarkBubbleGtk();

  static gboolean HandleDestroyThunk(GtkWidget* widget,
                                     gpointer userdata) {
    return reinterpret_cast<BookmarkBubbleGtk*>(userdata)->
        HandleDestroy();
  }
  // Notified when |content_| is destroyed so we can delete our instance.
  gboolean HandleDestroy();
  
  // The URL of the bookmark.
  GURL url_;
  // Our current profile (used to access the bookmark system).
  Profile* profile_;
  // Whether the bubble is creating or editing an existing bookmark.
  bool newly_bookmarked_;

  // We let the InfoBubble own our content, and then we delete ourself
  // when the widget is destroyed (when the InfoBubble is destroyed).
  GtkWidget* content_;

  // The combo box for selecting the bookmark folder.
  GtkWidget* combo_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleGtk);
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_
