// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/list_store_favicon_loader.h"

#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "base/gfx/png_decoder.h"
#include "chrome/browser/profile.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

ListStoreFavIconLoader::ListStoreFavIconLoader(
    GtkListStore* list_store, gint favicon_col, gint favicon_handle_col,
    Profile* profile, CancelableRequestConsumer* consumer)
    : list_store_(list_store), favicon_col_(favicon_col),
      favicon_handle_col_(favicon_handle_col), profile_(profile),
      consumer_(consumer) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  default_favicon_ = rb.GetPixbufNamed(IDR_DEFAULT_FAVICON);
}

void ListStoreFavIconLoader::LoadFaviconForRow(const GURL& url,
                                               GtkTreeIter* iter) {
  HistoryService* history =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history) {
    HistoryService::Handle handle = history->GetFavIconForURL(
        url, consumer_,
        NewCallback(this, &ListStoreFavIconLoader::OnGotFavIcon));
    gtk_list_store_set(list_store_, iter,
                       favicon_handle_col_, handle,
                       favicon_col_, default_favicon_,
                       -1);
  }
}

bool ListStoreFavIconLoader::GetRowByFavIconHandle(
    HistoryService::Handle handle, GtkTreeIter* result_iter) {
  GtkTreeIter iter;
  gboolean valid = gtk_tree_model_get_iter_first(
      GTK_TREE_MODEL(list_store_), &iter);
  while (valid) {
    gint row_handle;
    gtk_tree_model_get(GTK_TREE_MODEL(list_store_), &iter,
                       favicon_handle_col_, &row_handle,
                       -1);
    if (row_handle == handle) {
      *result_iter = iter;
      return true;
    }
    valid = gtk_tree_model_iter_next(
        GTK_TREE_MODEL(list_store_), &iter);
  }
  return false;
}

void ListStoreFavIconLoader::OnGotFavIcon(
    HistoryService::Handle handle, bool know_fav_icon,
    scoped_refptr<RefCountedBytes> image_data, bool is_expired, GURL icon_url) {
  GtkTreeIter iter;
  if (!GetRowByFavIconHandle(handle, &iter))
    return;
  gtk_list_store_set(list_store_, &iter,
                     favicon_handle_col_, 0,
                     -1);
  if (know_fav_icon && image_data.get() && !image_data->data.empty()) {
    int width, height;
    std::vector<unsigned char> decoded_data;
    if (PNGDecoder::Decode(&image_data->data.front(), image_data->data.size(),
                           PNGDecoder::FORMAT_BGRA, &decoded_data, &width,
                           &height)) {
      SkBitmap icon;
      icon.setConfig(SkBitmap::kARGB_8888_Config, width, height);
      icon.allocPixels();
      memcpy(icon.getPixels(), &decoded_data.front(),
             width * height * 4);
      GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
      gtk_list_store_set(list_store_, &iter,
                         favicon_col_, pixbuf,
                         -1);
      g_object_unref(pixbuf);
    }
  }
}
