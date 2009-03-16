// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/menu_gtk.h"

#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/l10n_util.h"
#include "skia/include/SkBitmap.h"

namespace {

// GTK uses _ for accelerators.  Windows uses & with && as an escape for &.
std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label) {
  std::string ret;
  ret.reserve(label.length());
  for (size_t i = 0; i < label.length(); ++i) {
    if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back(label[i]);
        ++i;
      } else {
        ret.push_back('_');
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

}  // namespace

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate,
                 const MenuCreateMaterial* menu_data,
                 GtkAccelGroup* accel_group)
    : delegate_(delegate),
      accel_group_(accel_group),
      menu_(gtk_menu_new()) {
  BuildMenuIn(menu_.get(), menu_data);
}

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate)
    : delegate_(delegate),
      menu_(gtk_menu_new()) {
  BuildMenuFromDelegate();
}

MenuGtk::~MenuGtk() {
  menu_.Destroy();
}

void MenuGtk::Popup(GtkWidget* widget, GdkEvent* event) {
  DCHECK(event->type == GDK_BUTTON_PRESS)
      << "Non-button press event sent to RunMenuAt";

  GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
  Popup(widget, event_button->button, event_button->time);
}

void MenuGtk::Popup(GtkWidget* widget, gint button_type, guint32 timestamp) {
  gtk_container_foreach(GTK_CONTAINER(menu_.get()), SetMenuItemInfo, this);

  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL,
                 MenuPositionFunc,
                 widget,
                 button_type, timestamp);
}

void MenuGtk::PopupAsContext() {
  gtk_container_foreach(GTK_CONTAINER(menu_.get()), SetMenuItemInfo, this);

  // TODO(estade): |button| value of 0 (6th argument) is not strictly true,
  // but does it matter?
  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL, NULL, NULL, 0,
                 gtk_get_current_event_time());
}

void MenuGtk::BuildMenuIn(GtkWidget* menu,
                          const MenuCreateMaterial* menu_data) {
  // We keep track of the last menu item in order to group radio items.
  GtkWidget* last_menu_item = NULL;
  for (; menu_data->type != MENU_END; ++menu_data) {
    GtkWidget* menu_item = NULL;

    std::string label;
    if (menu_data->label_argument) {
      label = WideToUTF8(l10n_util::GetStringF(
          menu_data->label_id,
          l10n_util::GetString(menu_data->label_argument)));
    } else if (menu_data->label_id) {
      label = WideToUTF8(l10n_util::GetString(menu_data->label_id));
    } else if (menu_data->type != MENU_SEPARATOR) {
      label = delegate_->GetLabel(menu_data->id);
      DCHECK(!label.empty());
    }

    label = ConvertAcceleratorsFromWindowsStyle(label);

    switch (menu_data->type) {
      case MENU_RADIO:
        if (GTK_IS_RADIO_MENU_ITEM(last_menu_item)) {
          menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
              GTK_RADIO_MENU_ITEM(last_menu_item), label.c_str());
        } else {
          menu_item = gtk_radio_menu_item_new_with_mnemonic(
              NULL, label.c_str());
        }
        break;
      case MENU_CHECKBOX:
        menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
        break;
      case MENU_SEPARATOR:
        menu_item = gtk_separator_menu_item_new();
        break;
      case MENU_NORMAL:
      default:
        menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());
        break;
    }

    if (menu_data->submenu) {
      GtkWidget* submenu = gtk_menu_new();
      BuildMenuIn(submenu, menu_data->submenu);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }

    if (accel_group_ && menu_data->accel_key) {
      // If we ever want to let the user do any key remaping, we'll need to
      // change the following so we make a gtk_accel_map which keeps the actual
      // keys.
      gtk_widget_add_accelerator(menu_item,
                                 "activate",
                                 accel_group_,
                                 menu_data->accel_key,
                                 GdkModifierType(menu_data->accel_modifiers),
                                 GTK_ACCEL_VISIBLE);
    }

    g_object_set_data(G_OBJECT(menu_item), "menu-data",
                      const_cast<MenuCreateMaterial*>(menu_data));

    g_signal_connect(G_OBJECT(menu_item), "activate",
                     G_CALLBACK(OnMenuItemActivated), this);

    gtk_widget_show(menu_item);
    gtk_menu_append(menu, menu_item);
    last_menu_item = menu_item;
  }
}

void MenuGtk::BuildMenuFromDelegate() {
  // Note that the menu IDs start at 1, not 0.
  for (int i = 1; i <= delegate_->GetItemCount(); ++i) {
    GtkWidget* menu_item = NULL;

    if (delegate_->IsItemSeparator(i)) {
      menu_item = gtk_separator_menu_item_new();
    } else if (delegate_->HasIcon(i)) {
      menu_item = gtk_image_menu_item_new_with_label(
          delegate_->GetLabel(i).c_str());
      const SkBitmap* icon = delegate_->GetIcon(i);
      GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
      GtkWidget* widget = gtk_image_new_from_pixbuf(pixbuf);
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), widget);
      g_object_unref(pixbuf);
    } else {
      menu_item = gtk_menu_item_new_with_label(delegate_->GetLabel(i).c_str());
    }

    g_object_set_data(G_OBJECT(menu_item), "menu-id",
                      reinterpret_cast<void*>(i));

    g_signal_connect(G_OBJECT(menu_item), "activate",
                     G_CALLBACK(OnMenuItemActivatedById), this);

    gtk_widget_show(menu_item);
    // TODO(estade): gtk_menu_append is deprecated.
    gtk_menu_append(menu_.get(), menu_item);
  }
}

// static
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

// static
void MenuGtk::OnMenuItemActivatedById(GtkMenuItem* menuitem, MenuGtk* menu) {
  // We receive activation messages when highlighting a menu that has a
  // submenu. Ignore them.
  if (!gtk_menu_item_get_submenu(menuitem)) {
    int id = reinterpret_cast<int>(
        g_object_get_data(G_OBJECT(menuitem), "menu-id"));
    menu->delegate_->ExecuteCommand(id);
  }
}

// static
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
  *x += widget->allocation.x;
  *y += widget->allocation.y + widget->allocation.height;

  // g_object_get_data() returns NULL if no such object is found. |left_align|
  // acts as a boolean, but we can't actually cast it to bool because gcc
  // complains about losing precision.
  void* left_align =
      g_object_get_data(G_OBJECT(widget), "left-align-popup");

  if (!left_align)
    *x += widget->allocation.width - menu_req.width;

  // TODO(erg): Deal with this scrolling off the bottom of the screen.

  // Regretfully, we can't rely on push_in to alter the coordinates above to
  // always make the menu fit on screen. It'd make the above calculations just
  // work though...
  *push_in = FALSE;
}

// static
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
