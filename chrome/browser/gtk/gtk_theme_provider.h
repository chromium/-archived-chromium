// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
#define CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_

#include "chrome/browser/browser_theme_provider.h"

#include "skia/ext/skia_utils.h"

class Profile;

typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;

// Specialization of BrowserThemeProvider which supplies system colors.
class GtkThemeProvider : public BrowserThemeProvider {
 public:
  GtkThemeProvider();
  virtual ~GtkThemeProvider();

  // Overridden from BrowserThemeProvider:
  //
  // Sets that we aren't using the system theme, then calls
  // BrowserThemeProvider's implementation.
  virtual void SetTheme(Extension* extension);
  virtual void UseDefaultTheme();
  virtual void SetNativeTheme();

  // Whether we should use the GTK system theme.
  static bool UseSystemThemeGraphics(Profile* profile);

 private:
  // Load theme data from preferences, possibly picking colors from GTK.
  virtual void LoadThemePrefs();

  // Possibly creates a theme specific version of theme_toolbar_default.
  // (minimally acceptable version right now, which is just a fill of the bg
  // color; this should instead invoke gtk_draw_box(...) for complex theme
  // engines.)
  virtual SkBitmap* LoadThemeBitmap(int id);

  // Handles signal from GTK that our theme has been changed.
  static void OnStyleSet(GtkWidget* widget,
                         GtkStyle* previous_style,
                         GtkThemeProvider* provider);

  void LoadGtkValues();

  // Sets the underlying theme colors/tints from a GTK color.
  void SetThemeColorFromGtk(const char* id, GdkColor* color);
  void SetThemeTintFromGtk(const char* id, GdkColor* color,
                           const skia::HSL& default_tint);

  // A GtkWidget that exists only so we can look at its properties (and take
  // its colors).
  GtkWidget* fake_window_;
};

#endif  // CHROME_BROWSER_GTK_GTK_THEME_PROVIDER_H_
