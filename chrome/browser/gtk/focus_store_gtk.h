// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_FOCUS_STORE_GTK_H_
#define CHROME_BROWSER_GTK_FOCUS_STORE_GTK_H_

#include "base/basictypes.h"

typedef struct _GtkWidget GtkWidget;

class FocusStoreGtk {
 public:
  FocusStoreGtk();

  virtual ~FocusStoreGtk();

  GtkWidget* widget() const { return widget_; }

  // Save the widget that is currently focused for |widget|'s toplevel (NOT
  // |widget|). Call with NULL to clear |widget_|.
  void Store(GtkWidget* widget);

 private:
  // Disconnect the previous destroy handler (if any).
  void DisconnectDestroyHandler();

  // The widget which last had focus.
  GtkWidget* widget_;

  // The widget for which we've stored focus might be destroyed by the time we
  // want to restore focus. Thus we connect to the "destroy" signal on that
  // widget. This is the ID for the destroy handler.
  unsigned int destroy_handler_id_;

  DISALLOW_COPY_AND_ASSIGN(FocusStoreGtk);
};

#endif  // CHROME_BROWSER_GTK_FOCUS_STORE_GTK_H_
