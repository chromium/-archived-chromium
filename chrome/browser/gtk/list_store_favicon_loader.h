// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_LIST_STORE_FAVICON_LOADER_H_
#define CHROME_BROWSER_GTK_LIST_STORE_FAVICON_LOADER_H_

#include <gtk/gtk.h>

#include "chrome/browser/history/history.h"
#include "googleurl/src/gurl.h"

class Profile;

// Handles loading favicons into a GDK_TYPE_PIXBUF column of a GtkListStore.
// The GtkListStore must also have a G_TYPE_INT column, passed as
// |favicon_handle_col|, which will be used internally by the loader.
// Note: this implementation will be inefficient if the GtkListStore has a large
// number of rows.
class ListStoreFavIconLoader {
 public:
  ListStoreFavIconLoader(GtkListStore* list_store,
                         gint favicon_col,
                         gint favicon_handle_col,
                         Profile* profile,
                         CancelableRequestConsumer* consumer);

  // Start loading the favicon for |url| into the row |iter|.
  void LoadFaviconForRow(const GURL& url, GtkTreeIter* iter);

 private:
  // Find a row from the GetFavIconForURL handle.  Returns true if the row was
  // found.
  bool GetRowByFavIconHandle(HistoryService::Handle handle,
                             GtkTreeIter* result_iter);

  // Callback from HistoryService:::GetFavIconForURL
  void OnGotFavIcon(HistoryService::Handle handle, bool know_fav_icon,
                    scoped_refptr<RefCountedBytes> image_data, bool is_expired,
                    GURL icon_url);

  // The list store we are loading favicons into.
  GtkListStore* list_store_;

  // The index of the GDK_TYPE_PIXBUF column to receive the favicons.
  gint favicon_col_;

  // The index of the G_TYPE_INT column used internally to track the
  // HistoryService::Handle of each favicon request.
  gint favicon_handle_col_;

  // The profile from which we will get the HistoryService.
  Profile* profile_;

  // Used in loading favicons.
  CancelableRequestConsumer* consumer_;

  // Default icon to show when one can't be found for the URL.  This is owned by
  // the ResourceBundle and we do not need to free it.
  GdkPixbuf* default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(ListStoreFavIconLoader);
};

#endif  // CHROME_BROWSER_GTK_LIST_STORE_FAVICON_LOADER_H_
