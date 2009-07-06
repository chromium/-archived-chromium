// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates a link button that shows |text| in blue and underlined. The cursor
// changes to a hand when over the link.  This is like the GTK LinkButton, but
// it doesn't call the global URI link handler, etc.  It is a button subclass,
// so you can just handle the clicked signal.

#ifndef CHROME_BROWSER_GTK_GTK_CHROME_LINK_BUTTON_H_
#define CHROME_BROWSER_GTK_GTK_CHROME_LINK_BUTTON_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_CHROME_LINK_BUTTON        (gtk_chrome_link_button_get_type ())
#define GTK_CHROME_LINK_BUTTON(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                            GTK_TYPE_CHROME_LINK_BUTTON, \
                                            GtkChromeLinkButton))
#define GTK_CHROME_LINK_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
                                             GTK_TYPE_CHROME_LINK_BUTTON, \
                                             GtkChromeLinkButtonClass))
#define GTK_IS_CHROME_LINK_BUTTON(obj)                           \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CHROME_LINK_BUTTON))
#define GTK_IS_CHROME_LINK_BUTTON_CLASS(klass)                   \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CHROME_LINK_BUTTON))
#define GTK_CHROME_LINK_BUTTON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  GTK_TYPE_CHROME_LINK_BUTTON, \
  GtkChromeLinkButton))

typedef struct _GtkChromeLinkButton        GtkChromeLinkButton;
typedef struct _GtkChromeLinkButtonClass   GtkChromeLinkButtonClass;

struct _GtkChromeLinkButton {
  GtkButton button;
  GtkWidget* label;
  gchar* blue_markup;
  gchar* red_markup;
  gboolean is_blue;
  GdkCursor* hand_cursor;
  GdkEventButton* click_button_event;
};

struct _GtkChromeLinkButtonClass {
  GtkButtonClass parent_class;
};

// Make a link button with display text |text|.
GtkWidget* gtk_chrome_link_button_new(const char* text);

// As above, but don't escape markup in the text.
GtkWidget* gtk_chrome_link_button_new_with_markup(const char* markup);

// Call this from within a "clicked" handler to get the release event that
// triggered the click. It will return NULL if the click was triggered by a
// keyboard event.
const GdkEventButton* gtk_chrome_link_button_get_event_for_click(
    GtkChromeLinkButton* button);

GType gtk_chrome_link_button_get_type();

G_END_DECLS

#endif  // CHROME_BROWSER_GTK_GTK_CHROME_LINK_BUTTON_H_
