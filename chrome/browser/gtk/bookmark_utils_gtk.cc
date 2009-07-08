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
#include "chrome/browser/gtk/gtk_theme_provider.h"
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

void* AsVoid(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // namespace

namespace bookmark_utils {

const char kBookmarkNode[] = "bookmark-node";

const int kBarButtonPadding = 4;

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
                                 BookmarkModel* model,
                                 GtkThemeProperties* properties) {
  // Build a windowed representation for our button.
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  if (!properties->use_gtk_rendering) {
    // TODO(erg): Theme wise, which color should I be picking here?
    // COLOR_BUTTON_BACKGROUND doesn't match the default theme!
    gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &kBackgroundColor);
  }
  gtk_widget_realize(window);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add(GTK_CONTAINER(window), frame);
  gtk_widget_show(frame);

  GtkWidget* floating_button = gtk_chrome_button_new();
  bookmark_utils::ConfigureButtonForNode(node, model, floating_button,
                                         properties);
  gtk_container_add(GTK_CONTAINER(frame), floating_button);
  gtk_widget_show(floating_button);

  return window;
}

void ConfigureButtonForNode(const BookmarkNode* node, BookmarkModel* model,
                            GtkWidget* button, GtkThemeProperties* properties) {
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(button));
  if (former_child)
    gtk_container_remove(GTK_CONTAINER(button), former_child);

  std::string tooltip = BuildTooltipFor(node);
  if (!tooltip.empty())
    gtk_widget_set_tooltip_text(button, tooltip.c_str());

  // We pack the button manually (rather than using gtk_button_set_*) so that
  // we can have finer control over its label.
  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model);
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);

  GtkWidget* label = gtk_label_new(WideToUTF8(node->GetTitle()).c_str());
  gtk_label_set_max_width_chars(GTK_LABEL(label), kMaxCharsOnAButton);
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);

  GtkWidget* box = gtk_hbox_new(FALSE, kBarButtonPadding);
  gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(button), box);

  SetButtonTextColors(label, properties);
  g_object_set_data(G_OBJECT(button), bookmark_utils::kBookmarkNode,
                    AsVoid(node));

  gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button),
                                          properties->use_gtk_rendering);

  gtk_widget_show_all(box);
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

void SetButtonTextColors(GtkWidget* label, GtkThemeProperties* properties) {
  if (properties->use_gtk_rendering) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, NULL);
    gtk_widget_modify_fg(label, GTK_STATE_PRELIGHT, NULL);
    gtk_widget_modify_fg(label, GTK_STATE_INSENSITIVE, NULL);
  } else {
    GdkColor color = properties->GetGdkColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &color);
    gtk_widget_modify_fg(label, GTK_STATE_ACTIVE, &color);
    gtk_widget_modify_fg(label, GTK_STATE_PRELIGHT, &color);
    gtk_widget_modify_fg(label, GTK_STATE_INSENSITIVE, &color);
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
    case GtkDndUtil::X_CHROME_TEXT_URI_LIST: {
      gchar** uris = reinterpret_cast<gchar**>(malloc(sizeof(gchar*) *
                                               (nodes.size() + 1)));
      for (size_t i = 0; i < nodes.size(); ++i) {
        // If the node is a folder, this will be empty. TODO(estade): figure out
        // if there are any ramifications to passing an empty URI. After a
        // lttle testing, it seems fine.
        const GURL& url = nodes[i]->GetURL();
        // This const cast should be safe as gtk_selection_data_set_uris()
        // makes copies.
        uris[i] = const_cast<gchar*>(url.spec().c_str());
      }
      uris[nodes.size()] = NULL;

      gtk_selection_data_set_uris(selection_data, uris);
      free(uris);
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
