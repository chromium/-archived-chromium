// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_font.h"

#include <gtk/gtk.h>

#include "base/string_util.h"

ChromeFont* ChromeFont::default_font_ = NULL;

// Get the default gtk system font (name and size).
// TODO(estade): is there a way to do this that does not involve making a
// temporary widget?
ChromeFont::ChromeFont() {
  if (default_font_ == NULL) {
    GtkWidget* widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    PangoFontDescription* desc = pango_context_get_font_description(
        gtk_widget_get_pango_context(widget));
    gint size = pango_font_description_get_size(desc);
    const char* name = pango_font_description_get_family(desc);

    default_font_ = new ChromeFont(CreateFont(UTF8ToWide(name), size));

    gtk_widget_destroy(widget);

    DCHECK(default_font_);
  }

  CopyChromeFont(*default_font_);
}

