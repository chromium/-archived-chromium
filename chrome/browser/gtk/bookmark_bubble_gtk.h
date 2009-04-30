// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

typedef struct _GtkWidget GtkWidget;

class Profile;
namespace gfx {
class Rect;
}

class BookmarkBubbleGtk {
 public:
  static void Show(const gfx::Rect& rect,
                   Profile* profile,
                   const GURL& url,
                   bool newly_bookmarked);
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_BUBBLE_GTK_H_
