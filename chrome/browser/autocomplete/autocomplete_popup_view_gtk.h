// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_GTK_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditModel;
class AutocompleteEditViewGtk;
class AutocompletePopupModel;
class Profile;
class SkBitmap;

class AutocompletePopupViewGtk : public AutocompletePopupView {
 public:
  AutocompletePopupViewGtk(AutocompleteEditViewGtk* edit_view,
                           AutocompleteEditModel* edit_model,
                           Profile* profile,
                           AutocompletePopupPositioner* popup_positioner);
  ~AutocompletePopupViewGtk();

  // Implement the AutocompletePopupView interface.
  virtual bool IsOpen() const { return opened_; }
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual void OnHoverEnabledOrDisabled(bool disabled);
  virtual void PaintUpdatesNow();
  virtual AutocompletePopupModel* GetModel();

 private:
  void Show(size_t num_results);
  void Hide();

  // Convert a y-coordinate to the closest line / result.
  size_t LineFromY(int y);

  // Accept a line of the results, for example, when the user clicks a line.
  void AcceptLine(size_t line, WindowOpenDisposition disposition);

  static gboolean HandleExposeThunk(GtkWidget* widget, GdkEventExpose* event,
                                    gpointer userdata) {
    return reinterpret_cast<AutocompletePopupViewGtk*>(userdata)->
        HandleExpose(widget, event);
  }
  gboolean HandleExpose(GtkWidget* widget, GdkEventExpose* event);

  static gboolean HandleMotionThunk(GtkWidget* widget, GdkEventMotion* event,
                                    gpointer userdata) {
    return reinterpret_cast<AutocompletePopupViewGtk*>(userdata)->
        HandleMotion(widget, event);
  }
  gboolean HandleMotion(GtkWidget* widget, GdkEventMotion* event);

  static gboolean HandleButtonPressThunk(GtkWidget* widget,
                                         GdkEventButton* event,
                                         gpointer userdata) {
    return reinterpret_cast<AutocompletePopupViewGtk*>(userdata)->
        HandleButtonPress(widget, event);
  }
  gboolean HandleButtonPress(GtkWidget* widget, GdkEventButton* event);

  static gboolean HandleButtonReleaseThunk(GtkWidget* widget,
                                           GdkEventButton* event,
                                           gpointer userdata) {
    return reinterpret_cast<AutocompletePopupViewGtk*>(userdata)->
        HandleButtonRelease(widget, event);
  }
  gboolean HandleButtonRelease(GtkWidget* widget, GdkEventButton* event);

  scoped_ptr<AutocompletePopupModel> model_;
  AutocompleteEditViewGtk* edit_view_;
  AutocompletePopupPositioner* popup_positioner_;

  // Our popup window, which is the only widget used, and we paint it on our
  // own.  This widget shouldn't be exposed outside of this class.
  GtkWidget* window_;
  // The pango layout object created from the window, cached across exposes.
  PangoLayout* layout_;

  // Whether our popup is currently open / shown, or closed / hidden.
  bool opened_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupViewGtk);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_GTK_H_
