// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_
#define VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_

#include <gtk/gtk.h>

#include "views/controls/menu/menu_wrapper.h"

namespace views {

class Menu2Model;

// A Gtk implementation of MenuWrapper.
// TODO(beng): rename to MenuGtk once the old class is dead.
class NativeMenuGtk : public MenuWrapper {
 public:
  explicit NativeMenuGtk(Menu2Model* model);
  virtual ~NativeMenuGtk();

  // Overridden from MenuWrapper:
  virtual void RunMenuAt(const gfx::Point& point, int alignment);
  virtual void CancelMenu();
  virtual void Rebuild();
  virtual void UpdateStates();
  virtual gfx::NativeMenu GetNativeMenu() const;

 private:
  void AddSeparatorAt(int index);
  void AddMenuItemAt(int index, GtkRadioMenuItem** last_radio_item);

  static void UpdateStateCallback(GtkWidget* menu_item, gpointer data);

  void ResetMenu();

  // Callback for gtk_menu_popup to position the menu.
  static void MenuPositionFunc(GtkMenu* menu, int* x, int* y, gboolean* push_in,
                               void* data);

  // Event handlers:
  void OnActivate(GtkMenuItem* menu_item);

  // Gtk signal handlers.
  static void CallActivate(GtkMenuItem* menu_item, NativeMenuGtk* native_menu);

  Menu2Model* model_;

  GtkWidget* menu_;

  DISALLOW_COPY_AND_ASSIGN(NativeMenuGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_
