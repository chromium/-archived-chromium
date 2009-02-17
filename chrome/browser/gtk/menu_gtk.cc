// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/menu_gtk.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/l10n_util.h"

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate,
                 const MenuCreateMaterial* menu_data)
    : delegate_(delegate),
      menu_(gtk_menu_new()) {
  BuildMenuIn(menu_, menu_data);
}

MenuGtk::~MenuGtk() {
  g_object_unref(menu_);
}

void MenuGtk::Popup(GtkWidget* widget, GdkEvent* event) {
  DCHECK(event->type == GDK_BUTTON_PRESS)
      << "Non-button press event sent to RunMenuAt";

  gtk_container_foreach(GTK_CONTAINER(menu_), SetMenuItemInfo, this);

  GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 MenuPositionFunc,
                 widget,
                 event_button->button, event_button->time);
}

void MenuGtk::BuildMenuIn(GtkWidget* menu,
                          const MenuCreateMaterial* menu_data) {
  for (; menu_data->type != MENU_END; menu_data++) {
    GtkWidget* menu_item = NULL;

    std::wstring label;
    if (menu_data->label_argument) {
      label = l10n_util::GetStringF(
          menu_data->label_id,
          l10n_util::GetString(menu_data->label_argument));
    } else if (menu_data->label_id) {
      label = l10n_util::GetString(menu_data->label_id);
    } else {
      DCHECK(menu_data->type == MENU_SEPARATOR) << "Menu definition broken";
    }

    switch (menu_data->type) {
      case MENU_CHECKBOX:
        menu_item = gtk_check_menu_item_new_with_label(
            WideToUTF8(label).c_str());
        break;
      case MENU_SEPARATOR:
        menu_item = gtk_separator_menu_item_new();
        break;
      case MENU_NORMAL:
      default:
        menu_item = gtk_menu_item_new_with_label(WideToUTF8(label).c_str());
        break;
    }

    if (menu_data->submenu) {
      GtkWidget* submenu = gtk_menu_new();
      BuildMenuIn(submenu, menu_data->submenu);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }

    g_object_set_data(G_OBJECT(menu_item), "menu-data",
                      const_cast<MenuCreateMaterial*>(menu_data));

    g_signal_connect(G_OBJECT(menu_item), "activate",
                     G_CALLBACK(OnMenuItemActivated), this);

    gtk_widget_show(menu_item);
    gtk_menu_append(menu, menu_item);
  }
}

/* static */
void MenuGtk::OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu) {
  // We receive activation messages when highlighting a menu that has a
  // submenu. Ignore them.
  if (!gtk_menu_item_get_submenu(menuitem)) {
    const MenuCreateMaterial* data =
        reinterpret_cast<const MenuCreateMaterial*>(
            g_object_get_data(G_OBJECT(menuitem), "menu-data"));
    menu->delegate_->ExecuteCommand(data->id);
  }
}

/* static */
void MenuGtk::MenuPositionFunc(GtkMenu* menu,
                               int* x,
                               int* y,
                               gboolean* push_in,
                               void* void_widget) {
  GtkWidget* widget = GTK_WIDGET(void_widget);
  GtkRequisition menu_req;
  GdkRectangle monitor;

  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);

  GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(menu));
  gint monitor_num = gdk_screen_get_monitor_at_window(screen, widget->window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor);

  gdk_window_get_origin(widget->window, x, y);
  *x += widget->allocation.x + widget->allocation.width;
  *y += widget->allocation.y + widget->allocation.height;

  *x -= menu_req.width;

  // TODO(erg): Deal with this scrolling off the bottom of the screen.

  // Regretfully, we can't rely on push_in to alter the coordinates above to
  // always make the menu fit on screen. It'd make the above calculations just
  // work though...
  *push_in = FALSE;
}

/* static */
void MenuGtk::SetMenuItemInfo(GtkWidget* widget, void* raw_menu) {
  MenuGtk* menu = static_cast<MenuGtk*>(raw_menu);
  const MenuCreateMaterial* data =
      reinterpret_cast<const MenuCreateMaterial*>(
          g_object_get_data(G_OBJECT(widget), "menu-data"));

  if (data) {
    if (GTK_IS_CHECK_MENU_ITEM(widget)) {
      GtkCheckMenuItem* item = GTK_CHECK_MENU_ITEM(widget);
      gtk_check_menu_item_set_active(
          item, menu->delegate_->IsItemChecked(data->id));
    }

    if (GTK_IS_MENU_ITEM(widget)) {
      gtk_widget_set_sensitive(
          widget, menu->delegate_->IsCommandEnabled(data->id));

      GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
      if (submenu) {
        gtk_container_foreach(GTK_CONTAINER(submenu), &MenuGtk::SetMenuItemInfo,
                              raw_menu);
      }
    }
  }
}
