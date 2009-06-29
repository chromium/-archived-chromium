// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_utils_gtk.h"

#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/profile.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Used in gtk_selection_data_set(). (I assume from this parameter that gtk has
// to some really exotic hardware...)
const int kBitsInAByte = 8;

// Maximum number of characters on a bookmark button.
const size_t kMaxCharsOnAButton = 15;

// Only used for the background of the drag widget.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);

// Color of the button text, taken from TextButtonView.
const GdkColor kEnabledColor = GDK_COLOR_RGB(6, 45, 117);
const GdkColor kDisabledColor = GDK_COLOR_RGB(161, 161, 146);
// TextButtonView uses 255, 255, 255 with opacity of 200. We don't support
// transparent text though, so just use a slightly lighter version of
// kEnabledColor.
const GdkColor kHighlightColor = GDK_COLOR_RGB(56, 95, 167);

std::string DoubleUnderscores(const std::string& text) {
  std::string ret;
  ret.reserve(text.length() * 2);
  for (size_t i = 0; i < text.length(); ++i) {
    if ('_' == text[i]) {
      ret.push_back('_');
      ret.push_back('_');
    } else {
      ret.push_back(text[i]);
    }
  }

  return ret;
}

// Recursively search for label among the children of |widget|.
void SearchForLabel(GtkWidget* widget, gpointer data) {
  if (GTK_IS_LABEL(widget)) {
    *reinterpret_cast<GtkWidget**>(data) = widget;
  } else if (GTK_IS_CONTAINER(widget)) {
    gtk_container_foreach(GTK_CONTAINER(widget), SearchForLabel, data);
  }
}

void* AsVoid(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // namespace

namespace bookmark_utils {

const char kBookmarkNode[] = "bookmark-node";

GdkPixbuf* GetFolderIcon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* default_folder_icon = rb.GetPixbufNamed(
      IDR_BOOKMARK_BAR_FOLDER);
  return default_folder_icon;
}

GdkPixbuf* GetDefaultFavicon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* default_bookmark_icon = rb.GetPixbufNamed(
      IDR_DEFAULT_FAVICON);
  return default_bookmark_icon;
}

GdkPixbuf* GetPixbufForNode(const BookmarkNode* node, BookmarkModel* model) {
  GdkPixbuf* pixbuf;

  if (node->is_url()) {
    if (model->GetFavIcon(node).width() != 0) {
      pixbuf = gfx::GdkPixbufFromSkBitmap(&model->GetFavIcon(node));
    } else {
      pixbuf = GetDefaultFavicon();
      g_object_ref(pixbuf);
    }
  } else {
    pixbuf = GetFolderIcon();
    g_object_ref(pixbuf);
  }

  return pixbuf;
}

GtkWidget* GetDragRepresentation(const BookmarkNode* node,
                                 BookmarkModel* model) {
  // Build a windowed representation for our button.
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &kBackgroundColor);
  gtk_widget_realize(window);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add(GTK_CONTAINER(window), frame);
  gtk_widget_show(frame);

  GtkWidget* floating_button = gtk_chrome_button_new();
  bookmark_utils::ConfigureButtonForNode(node, model, floating_button);
  gtk_container_add(GTK_CONTAINER(frame), floating_button);
  gtk_widget_show(floating_button);

  return window;
}

void ConfigureButtonForNode(const BookmarkNode* node, BookmarkModel* model,
                            GtkWidget* button) {
  std::string tooltip = BuildTooltipFor(node);
  if (!tooltip.empty())
    gtk_widget_set_tooltip_text(button, tooltip.c_str());

  // TODO(erg): Consider a soft maximum instead of this hard 15.
  std::wstring title = node->GetTitle();
  // Don't treat underscores as mnemonics.
  // O, that we could just use gtk_button_set_use_underline()!
  // See http://bugzilla.gnome.org/show_bug.cgi?id=586330
  std::string text = DoubleUnderscores(WideToUTF8(title));
  text = text.substr(0, std::min(text.size(), kMaxCharsOnAButton));
  gtk_button_set_label(GTK_BUTTON(button), text.c_str());

  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model);
  gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);

  SetButtonTextColors(button);
  g_object_set_data(G_OBJECT(button), bookmark_utils::kBookmarkNode,
                    AsVoid(node));
}

std::string BuildTooltipFor(const BookmarkNode* node) {
  // TODO(erg): Actually build the tooltip. For now, we punt and just return
  // the URL.
  return node->GetURL().possibly_invalid_spec();
}

const BookmarkNode* BookmarkNodeForWidget(GtkWidget* widget) {
  return reinterpret_cast<const BookmarkNode*>(
      g_object_get_data(G_OBJECT(widget), bookmark_utils::kBookmarkNode));
}

void SetButtonTextColors(GtkWidget* button) {
  GtkWidget* label;
  gtk_container_foreach(GTK_CONTAINER(button), SearchForLabel, &label);
  if (label) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &kEnabledColor);
    gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, &kEnabledColor);
    gtk_widget_modify_fg(label, GTK_STATE_PRELIGHT, &kHighlightColor);
    gtk_widget_modify_fg(label, GTK_STATE_INSENSITIVE, &kDisabledColor);
  }
}

// DnD-related -----------------------------------------------------------------

void WriteBookmarkToSelection(const BookmarkNode* node,
                              GtkSelectionData* selection_data,
                              guint target_type,
                              Profile* profile) {
  DCHECK(node);
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  WriteBookmarksToSelection(nodes, selection_data, target_type, profile);
}

void WriteBookmarksToSelection(const std::vector<const BookmarkNode*>& nodes,
                               GtkSelectionData* selection_data,
                               guint target_type,
                               Profile* profile) {
  switch (target_type) {
    case GtkDndUtil::X_CHROME_BOOKMARK_ITEM: {
      BookmarkDragData data(nodes);
      Pickle pickle;
      data.WriteToPickle(profile, &pickle);

      gtk_selection_data_set(selection_data, selection_data->target,
                             kBitsInAByte,
                             static_cast<const guchar*>(pickle.data()),
                             pickle.size());
      break;
    }
    default: {
      DLOG(ERROR) << "Unsupported drag get type!";
    }
  }
}

std::vector<const BookmarkNode*> GetNodesFromSelection(
    GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type,
    Profile* profile,
    gboolean* delete_selection_data,
    gboolean* dnd_success) {
  *delete_selection_data = FALSE;
  *dnd_success = FALSE;

  if ((selection_data != NULL) && (selection_data->length >= 0)) {
    if (context->action == GDK_ACTION_MOVE) {
      *delete_selection_data = TRUE;
    }

    switch (target_type) {
      case GtkDndUtil::X_CHROME_BOOKMARK_ITEM: {
        *dnd_success = TRUE;
        Pickle pickle(reinterpret_cast<char*>(selection_data->data),
                      selection_data->length);
        BookmarkDragData drag_data;
        drag_data.ReadFromPickle(&pickle);
        return drag_data.GetNodes(profile);
      }
      default: {
        DLOG(ERROR) << "Unsupported drag received type: " << target_type;
      }
    }
  }

  return std::vector<const BookmarkNode*>();
}

}  // namespace bookmark_utils
