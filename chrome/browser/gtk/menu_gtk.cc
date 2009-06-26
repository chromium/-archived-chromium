// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/menu_gtk.h"

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/common/gtk_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

using gtk_util::ConvertAcceleratorsFromWindowsStyle;

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate,
                 const MenuCreateMaterial* menu_data,
                 GtkAccelGroup* accel_group)
    : delegate_(delegate),
      dummy_accel_group_(gtk_accel_group_new()),
      menu_(gtk_menu_new()) {
  ConnectSignalHandlers();
  BuildMenuIn(menu_.get(), menu_data, accel_group);
}

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate, bool load)
    : delegate_(delegate),
      dummy_accel_group_(NULL),
      menu_(gtk_menu_new()) {
  ConnectSignalHandlers();
  if (load)
    BuildMenuFromDelegate();
}

MenuGtk::~MenuGtk() {
  menu_.Destroy();
  if (dummy_accel_group_)
    g_object_unref(dummy_accel_group_);
}

void MenuGtk::ConnectSignalHandlers() {
  g_signal_connect(menu_.get(), "show", G_CALLBACK(OnMenuShow), this);
  g_signal_connect(menu_.get(), "hide", G_CALLBACK(OnMenuHidden), this);
}

void MenuGtk::AppendMenuItemWithLabel(int command_id,
                                      const std::string& label) {
  std::string converted_label = ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item =
      gtk_menu_item_new_with_mnemonic(converted_label.c_str());
  AddMenuItemWithId(menu_item, command_id);
}

void MenuGtk::AppendMenuItemWithIcon(int command_id,
                                     const std::string& label,
                                     const SkBitmap& icon) {
  GtkWidget* menu_item = BuildMenuItemWithImage(label, icon);
  AddMenuItemWithId(menu_item, command_id);
}

void MenuGtk::AppendCheckMenuItemWithLabel(int command_id,
                                           const std::string& label) {
  std::string converted_label = ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item =
      gtk_check_menu_item_new_with_mnemonic(converted_label.c_str());
  AddMenuItemWithId(menu_item, command_id);
}

void MenuGtk::AppendSeparator() {
  GtkWidget* menu_item = gtk_separator_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_.get()), menu_item);
}

void MenuGtk::Popup(GtkWidget* widget, GdkEvent* event) {
  DCHECK(event->type == GDK_BUTTON_PRESS)
      << "Non-button press event sent to RunMenuAt";

  GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
  Popup(widget, event_button->button, event_button->time);
}

void MenuGtk::Popup(GtkWidget* widget, gint button_type, guint32 timestamp) {
  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL,
                 MenuPositionFunc,
                 widget,
                 button_type, timestamp);
}

void MenuGtk::PopupAsContext(guint32 event_time) {
  // TODO(estade): |button| value of 3 (6th argument) is not strictly true,
  // but does it matter?
  gtk_menu_popup(GTK_MENU(menu_.get()), NULL, NULL, NULL, NULL, 3, event_time);
}

void MenuGtk::Cancel() {
  gtk_menu_popdown(GTK_MENU(menu_.get()));
}

void MenuGtk::BuildMenuIn(GtkWidget* menu,
                          const MenuCreateMaterial* menu_data,
                          GtkAccelGroup* accel_group) {
  // We keep track of the last menu item in order to group radio items.
  GtkWidget* last_menu_item = NULL;
  for (; menu_data->type != MENU_END; ++menu_data) {
    GtkWidget* menu_item = NULL;

    std::string label;
    if (menu_data->label_argument) {
      label = l10n_util::GetStringFUTF8(
          menu_data->label_id,
          WideToUTF16(l10n_util::GetString(menu_data->label_argument)));
    } else if (menu_data->label_id) {
      label = l10n_util::GetStringUTF8(menu_data->label_id);
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
      BuildMenuIn(submenu, menu_data->submenu, accel_group);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }

    if (accel_group && menu_data->accel_key) {
      // If we ever want to let the user do any key remaping, we'll need to
      // change the following so we make a gtk_accel_map which keeps the actual
      // keys.
      gtk_widget_add_accelerator(menu_item,
                                 "activate",
                                 menu_data->only_show ? dummy_accel_group_ :
                                                        accel_group,
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

GtkWidget* MenuGtk::BuildMenuItemWithImage(const std::string& label,
                                           const SkBitmap& icon) {
  std::string converted_label = ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item =
      gtk_image_menu_item_new_with_mnemonic(converted_label.c_str());

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);

  return menu_item;
}

void MenuGtk::BuildMenuFromDelegate() {
  // Note that the menu IDs start at 1, not 0.
  for (int i = 1; i <= delegate_->GetItemCount(); ++i) {
    GtkWidget* menu_item = NULL;

    if (delegate_->IsItemSeparator(i)) {
      menu_item = gtk_separator_menu_item_new();
    } else if (delegate_->HasIcon(i)) {
      const SkBitmap* icon = delegate_->GetIcon(i);
      menu_item = BuildMenuItemWithImage(delegate_->GetLabel(i), *icon);
    } else {
      menu_item = gtk_menu_item_new_with_label(delegate_->GetLabel(i).c_str());
    }

    AddMenuItemWithId(menu_item, i);
  }
}

void MenuGtk::AddMenuItemWithId(GtkWidget* menu_item, int id) {
  g_object_set_data(G_OBJECT(menu_item), "menu-id",
                    reinterpret_cast<void*>(id));

  g_signal_connect(G_OBJECT(menu_item), "activate",
                   G_CALLBACK(OnMenuItemActivatedById), this);

  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_.get()), menu_item);
}

// static
void MenuGtk::OnMenuItemActivated(GtkMenuItem* menuitem, MenuGtk* menu) {
  // We receive activation messages when highlighting a menu that has a
  // submenu. Ignore them.
  if (!gtk_menu_item_get_submenu(menuitem)) {
    const MenuCreateMaterial* data =
        reinterpret_cast<const MenuCreateMaterial*>(
            g_object_get_data(G_OBJECT(menuitem), "menu-data"));
    // The menu item can still be activated by hotkeys even if it is disabled.
    if (menu->delegate_->IsCommandEnabled(data->id))
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
    // The menu item can still be activated by hotkeys even if it is disabled.
    if (menu->delegate_->IsCommandEnabled(id))
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

  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);

  gdk_window_get_origin(widget->window, x, y);
  GdkScreen *screen = gtk_widget_get_screen(widget);
  gint monitor = gdk_screen_get_monitor_at_point(screen, *x, *y);

  GdkRectangle screen_rect;
  gdk_screen_get_monitor_geometry(screen, monitor,
                                  &screen_rect);

  if (GTK_WIDGET_NO_WINDOW(widget)) {
    *x += widget->allocation.x;
    *y += widget->allocation.y;
  }
  *y += widget->allocation.height;

  bool start_align =
    !!g_object_get_data(G_OBJECT(widget), "left-align-popup");
  if (gtk_widget_get_direction(GTK_WIDGET(menu)) == GTK_TEXT_DIR_RTL)
    start_align = !start_align;

  if (!start_align)
    *x += widget->allocation.width - menu_req.width;

  if (*y + menu_req.height >= screen_rect.height)
    *y -= menu_req.height;

  *push_in = FALSE;
}

// static
void MenuGtk::OnMenuShow(GtkWidget* widget, MenuGtk* menu) {
  gtk_container_foreach(GTK_CONTAINER(menu->menu_.get()),
                        SetMenuItemInfo, menu);
}

// static
void MenuGtk::OnMenuHidden(GtkWidget* widget, MenuGtk* menu) {
  menu->delegate_->StoppedShowing();
}

// static
void MenuGtk::SetMenuItemInfo(GtkWidget* widget, gpointer userdata) {
  if (GTK_IS_SEPARATOR_MENU_ITEM(widget)) {
    // We need to explicitly handle this case because otherwise we'll ask the
    // menu delegate about something with an invalid id.
    return;
  }

  MenuGtk* menu = reinterpret_cast<MenuGtk*>(userdata);
  int id;
  const MenuCreateMaterial* data =
      reinterpret_cast<const MenuCreateMaterial*>(
          g_object_get_data(G_OBJECT(widget), "menu-data"));
  if (data) {
    id = data->id;
  } else {
    id = reinterpret_cast<int>(
        g_object_get_data(G_OBJECT(widget), "menu-id"));
  }

  if (GTK_IS_CHECK_MENU_ITEM(widget)) {
    GtkCheckMenuItem* item = GTK_CHECK_MENU_ITEM(widget);

    // gtk_check_menu_item_set_active() will send the activate signal. Touching
    // the underlying "active" property will also call the "activate" handler
    // for this menu item. So we prevent the correct activate handler from
    // being called while we set the checkbox.
    if (data) {
      g_signal_handlers_block_by_func(
          item, reinterpret_cast<void*>(OnMenuItemActivated), userdata);
    } else {
      g_signal_handlers_block_by_func(
          item, reinterpret_cast<void*>(OnMenuItemActivatedById), userdata);
    }

    gtk_check_menu_item_set_active(
        item, menu->delegate_->IsItemChecked(id));

    if (data) {
      g_signal_handlers_unblock_by_func(
          item, reinterpret_cast<void*>(OnMenuItemActivated), userdata);
    } else {
      g_signal_handlers_unblock_by_func(
          item, reinterpret_cast<void*>(OnMenuItemActivatedById), userdata);
    }
  }

  if (GTK_IS_MENU_ITEM(widget)) {
    gtk_widget_set_sensitive(
        widget, menu->delegate_->IsCommandEnabled(id));

    GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
    if (submenu) {
      gtk_container_foreach(GTK_CONTAINER(submenu), &SetMenuItemInfo,
                            userdata);
    }
  }
}
