// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_PANEL_CONTROLLER_H_

#include <gtk/gtk.h>

#include "views/controls/button/button.h"

class BrowserWindowGtk;
typedef unsigned long XID;

namespace views {
class ImageButton;
class Label;
class MouseEvent;
class WidgetGtk;
}

// Controls interactions with the WM for popups / panels.
class PanelController : public views::ButtonListener {
 public:
  explicit PanelController(BrowserWindowGtk* browser_window);
  virtual ~PanelController() {}

  bool TitleMousePressed(const views::MouseEvent& event);
  void TitleMouseReleased(const views::MouseEvent& event, bool canceled);
  bool TitleMouseDragged(const views::MouseEvent& event);
  bool PanelClientEvent(GdkEventClient* event);

  void UpdateTitleBar();
  void Close();
  // ButtonListener methods.
  virtual void ButtonPressed(views::Button* sender);

 private:
  class TitleContentView : public views::View {
   public:
    explicit TitleContentView(PanelController* panelController);
    virtual ~TitleContentView() {}
    virtual void Layout();
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
    virtual bool OnMouseDragged(const views::MouseEvent& event);

    views::Label* title_label() { return title_label_; }
    views::ImageButton* close_button() { return close_button_; }

   private:
    views::Label* title_label_;
    views::ImageButton* close_button_;
    PanelController* panel_controller_;
    DISALLOW_COPY_AND_ASSIGN(TitleContentView);
  };

  // Dispatches client events to PanelController instances
  static bool OnPanelClientEvent(
      GtkWidget* widget,
      GdkEventClient* event,
      PanelController* panel_controller);

  // Browser window containing content.
  BrowserWindowGtk* browser_window_;
  // Gtk object for content.
  GtkWindow* panel_;
  // X id for content.
  XID panel_xid_;

  // Views object representing title.
  views::WidgetGtk* title_window_;
  // Gtk object representing title.
  GtkWidget* title_;
  // X id representing title.
  XID title_xid_;

  // Views object, holds title and close button.
  TitleContentView* title_content_;

  // Is the panel expanded or collapsed?
  bool expanded_;

  // Is the mouse button currently down?
  bool mouse_down_;

  // Cursor's absolute position when the mouse button was pressed.
  int mouse_down_abs_x_;
  int mouse_down_abs_y_;

  // Cursor's offset from the upper-left corner of the titlebar when the
  // mouse button was pressed.
  int mouse_down_offset_x_;
  int mouse_down_offset_y_;

  // Is the titlebar currently being dragged?  That is, has the cursor
  // moved more than kDragThreshold away from its starting position?
  bool dragging_;

  DISALLOW_COPY_AND_ASSIGN(PanelController);
};

#endif  // CHROME_BROWSER_PANEL_CONTROLLER_H_

