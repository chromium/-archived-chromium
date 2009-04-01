// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/chrome_font.h"

#include <gtk/gtk.h>

#include "base/string_util.h"

ChromeFont* ChromeFont::default_font_ = NULL;

// Get the default gtk system font (name and size).
ChromeFont::ChromeFont() {
  if (default_font_ == NULL) {
    gtk_init(NULL, NULL);
    GtkSettings* settings = gtk_settings_get_default();

    GValue value = {0};
    g_value_init(&value, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(settings), "gtk-font-name", &value);

    // gtk-font-name may be wrapped in quotes.
    gchar* font_name = g_strdup_value_contents(&value);
    gchar* font_ptr = font_name;
    if (font_ptr[0] == '\"')
      font_ptr++;
    if (font_ptr[strlen(font_ptr) - 1] == '\"')
      font_ptr[strlen(font_ptr) - 1] = '\0';

    PangoFontDescription* desc =
        pango_font_description_from_string(font_ptr);
    gint size = pango_font_description_get_size(desc);
    const char* name = pango_font_description_get_family(desc);

    default_font_ = new ChromeFont(CreateFont(UTF8ToWide(name),
                                   size / PANGO_SCALE));

    pango_font_description_free(desc);
    g_free(font_name);
    g_value_unset(&value);

    DCHECK(default_font_);
  }

  CopyChromeFont(*default_font_);
}
