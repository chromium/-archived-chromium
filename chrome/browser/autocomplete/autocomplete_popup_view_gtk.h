// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_GTK_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete.h"
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
                           Profile* profile);
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

  static gboolean HandleExposeThunk(GtkWidget* widget, GdkEventExpose* event,
                                    gpointer userdata) {
    return reinterpret_cast<AutocompletePopupViewGtk*>(userdata)->
        HandleExpose(widget, event);
  }

  gboolean HandleExpose(GtkWidget* widget, GdkEventExpose* event);

  PangoFontDescription* font_;

  scoped_ptr<AutocompletePopupModel> model_;
  AutocompleteEditViewGtk* edit_view_;

  GtkWidget* window_;

  bool opened_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupViewGtk);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_VIEW_GTK_H_
