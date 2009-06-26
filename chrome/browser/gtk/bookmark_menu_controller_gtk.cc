// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_menu_controller_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/bookmark_context_menu.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/common/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

void SetImageMenuItem(GtkWidget* menu_item, const SkBitmap& bitmap) {
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);
}

const BookmarkNode* GetNodeFromMenuItem(GtkWidget* menu_item) {
  return static_cast<const BookmarkNode*>(
      g_object_get_data(G_OBJECT(menu_item), "bookmark-node"));
}

void* AsVoid(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // namespace

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               Profile* profile,
                                               PageNavigator* navigator,
                                               GtkWindow* window,
                                               const BookmarkNode* node,
                                               int start_child_index,
                                               bool show_other_folder)
    : browser_(browser),
      profile_(profile),
      page_navigator_(navigator),
      parent_window_(window),
      node_(node) {
  menu_.Own(gtk_menu_new());
  BuildMenu(node, start_child_index, menu_.get());
  gtk_widget_show_all(menu_.get());
}

BookmarkMenuController::~BookmarkMenuController() {
  profile_->GetBookmarkModel()->RemoveObserver(this);
  menu_.Destroy();
}

void BookmarkMenuController::Popup(GtkWidget* widget, gint button_type,
                                   guint32 timestamp) {
  profile_->GetBookmarkModel()->AddObserver(this);

  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL,
                 &MenuGtk::MenuPositionFunc,
                 widget, button_type, timestamp);
}

void BookmarkMenuController::BookmarkModelChanged() {
  gtk_menu_popdown(GTK_MENU(menu_.get()));
}

void BookmarkMenuController::BookmarkNodeFavIconLoaded(
    BookmarkModel* model, const BookmarkNode* node) {
  std::map<const BookmarkNode*, GtkWidget*>::iterator it =
      node_to_menu_widget_map_.find(node);
  if (it != node_to_menu_widget_map_.end())
    SetImageMenuItem(it->second, model->GetFavIcon(node));
}

void BookmarkMenuController::NavigateToMenuItem(
    GtkWidget* menu_item,
    WindowOpenDisposition disposition) {
  const BookmarkNode* node = GetNodeFromMenuItem(menu_item);
  DCHECK(node);
  DCHECK(page_navigator_);
  page_navigator_->OpenURL(
      node->GetURL(), GURL(), disposition, PageTransition::AUTO_BOOKMARK);
}

void BookmarkMenuController::BuildMenu(const BookmarkNode* parent,
                                       int start_child_index,
                                       GtkWidget* menu) {
  DCHECK(!parent->GetChildCount() ||
         start_child_index < parent->GetChildCount());
  for (int i = start_child_index; i < parent->GetChildCount(); ++i) {
    const BookmarkNode* node = parent->GetChild(i);

    GtkWidget* menu_item = gtk_image_menu_item_new_with_label(
        WideToUTF8(node->GetTitle()).c_str());

    if (node->is_url()) {
      SkBitmap icon = profile_->GetBookmarkModel()->GetFavIcon(node);
      if (icon.width() == 0) {
        icon = *ResourceBundle::GetSharedInstance().
            GetBitmapNamed(IDR_DEFAULT_FAVICON);
      }
      SetImageMenuItem(menu_item, icon);
      g_object_set_data(G_OBJECT(menu_item), "bookmark-node", AsVoid(node));
      g_signal_connect(G_OBJECT(menu_item), "activate",
                       G_CALLBACK(OnMenuItemActivated), this);
      g_signal_connect(G_OBJECT(menu_item), "button-press-event",
                       G_CALLBACK(OnButtonPressed), this);
      g_signal_connect(G_OBJECT(menu_item), "button-release-event",
                       G_CALLBACK(OnButtonReleased), this);
    } else if (node->is_folder()) {
      SkBitmap* folder_icon = ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_BOOKMARK_BAR_FOLDER);
      SetImageMenuItem(menu_item, *folder_icon);

      GtkWidget* submenu = gtk_menu_new();
      BuildMenu(node, 0, submenu);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    } else {
      NOTREACHED();
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    node_to_menu_widget_map_[node] = menu_item;
  }

  if (parent->GetChildCount() == 0) {
    GtkWidget* empty_menu = gtk_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_MENU_EMPTY_SUBMENU).c_str());
    gtk_widget_set_sensitive(empty_menu, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), empty_menu);
  }
}

gboolean BookmarkMenuController::OnButtonPressed(
    GtkWidget* sender,
    GdkEventButton* event,
    BookmarkMenuController* controller) {
  if (event->button == 3) {
    // Show the right click menu and stop processing this button event.
    const BookmarkNode* node = GetNodeFromMenuItem(sender);
    const BookmarkNode* parent = node->GetParent();
    std::vector<const BookmarkNode*> nodes;
    nodes.push_back(node);
    controller->context_menu_.reset(
        new BookmarkContextMenu(
            sender, controller->profile_, controller->browser_,
            controller->page_navigator_, parent, nodes,
            BookmarkContextMenu::BOOKMARK_BAR));
    controller->context_menu_->PopupAsContext(event->time);
    return TRUE;
  }

  return FALSE;
}

gboolean BookmarkMenuController::OnButtonReleased(
    GtkWidget* sender,
    GdkEventButton* event,
    BookmarkMenuController* controller) {
  // TODO(erg): The OnButtonPressed and OnButtonReleased handlers should have
  // the same guard code that prevents them from interfering with DnD as
  // BookmarkBarGtk's versions.

  // Releasing either button 1 or 2 should trigger the bookmark menu.
  if (event->button == 1 || event->button == 2) {
    WindowOpenDisposition disposition =
        event_utils::DispositionFromEventFlags(event->state);
    controller->NavigateToMenuItem(sender, disposition);

    // We need to manually dismiss the popup menu because we're overriding
    // button-release-event.
    gtk_menu_popdown(GTK_MENU(controller->menu_.get()));
    return TRUE;
  }

  return FALSE;
}

void BookmarkMenuController::OnMenuItemActivated(
    GtkMenuItem* menu_item, BookmarkMenuController* controller) {
  controller->NavigateToMenuItem(GTK_WIDGET(menu_item), CURRENT_TAB);
}
