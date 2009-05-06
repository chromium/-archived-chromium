// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/chrome_font.h"

#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>

#include "base/string_util.h"

ChromeFont* ChromeFont::default_font_ = NULL;

// Find the best match font for |family_name| in the same way as Skia
// to make sure CreateFont() successfully creates default font.
// In Skia, it only checks the best match font.  If it failed to find,
// SkTypeface will be NULL for that font family.  It eventually causes segfault.
// For example, family_name = "Sans" and system may have various fonts.
// The first font family in FcPattern will be "DejaVu Sans" but a font family
// returned by FcFontMatch will be "VL PGothic".
// In this case, SkTypeface for "Sans" returns NULL even if system has font
// for "Sans" font family.
// See FontMatch() in skia/ports/SkFontHost_fontconfig.cpp for more detail.
static std::wstring FindBestMatchFontFamilyName(const char* family_name) {
    FcPattern* pattern = FcPatternCreate();
    FcValue fcvalue;
    fcvalue.type = FcTypeString;
    char* family_name_copy = strdup(family_name);
    fcvalue.u.s = reinterpret_cast<FcChar8*>(family_name_copy);
    FcPatternAdd(pattern, FC_FAMILY, fcvalue, 0);
    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern* match = FcFontMatch(0, pattern, &result);
    DCHECK(match) << "Could not find font: " << family_name;
    FcChar8* match_family;
    FcPatternGetString(match, FC_FAMILY, 0, &match_family);

    std::wstring font_family = UTF8ToWide(
        reinterpret_cast<char*>(match_family));
    FcPatternDestroy(match);
    FcPatternDestroy(pattern);
    free(family_name_copy);
    return font_family;
}

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
    const char* family_name = pango_font_description_get_family(desc);

    // Find best match font for |family_name| to make sure we can get
    // SkTypeface for default font.
    // TODO(agl): remove this.
    std::wstring font_family = FindBestMatchFontFamilyName(family_name);

    default_font_ = new ChromeFont(CreateFont(font_family, size / PANGO_SCALE));

    pango_font_description_free(desc);
    g_free(font_name);
    g_value_unset(&value);

    DCHECK(default_font_);
  }

  CopyChromeFont(*default_font_);
}
