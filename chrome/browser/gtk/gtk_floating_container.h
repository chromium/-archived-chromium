// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_GTK_FLOATING_CONTAINER_H_
#define CHROME_BROWSER_GTK_GTK_FLOATING_CONTAINER_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

// A specialized container, which is a cross between a GtkBin and a
// GtkFixed. This container dervies from GtkBin and the implementation of
// gtk_container_add() is the same: only one GtkWidget can be added through
// that interface. The GtkBin portion contains normal content and is given the
// same allocation that this container has.
//
// In addition, any number of widgets can be through the
// gtk_floating_container_add_floating() method, which provides functionality
// similar to a GtkFixed. Unlike a GtkFixed, coordinates are not set when you
// gtk_fixed_put(). The location of the floating widgets is determined while
// running the "set-floating-position" signal, which is emitted during this
// container's "size-allocate" handler.
//
// The "set-floating-position" signal is (semi-)mandatory if you want widgets
// placed anywhere other than the origin and should have the following
// signature:
//
//   void (*set_floating_position)(GtkFloatingContainer* container,
//                                 GtkAllocation* allocation);
//
// Your handler should, for each floating widget, set the "x" and "y" child
// properties.

G_BEGIN_DECLS

#define GTK_TYPE_FLOATING_CONTAINER                                 \
    (gtk_floating_container_get_type())
#define GTK_FLOATING_CONTAINER(obj)                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_FLOATING_CONTAINER, \
                                GtkFloatingContainer))
#define GTK_FLOATING_CONTAINER_CLASS(klass)                         \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_FLOATING_CONTAINER,  \
                             GtkFloatingContainerClass))
#define GTK_IS_FLOATING_CONTAINER(obj)                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_FLOATING_CONTAINER))
#define GTK_IS_FLOATING_CONTAINER_CLASS(klass)                      \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_FLOATING_CONTAINER))
#define GTK_FLOATING_CONTAINER_GET_CLASS(obj)                       \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_FLOATING_CONTAINER,  \
                               GtkFloatingContainerClass))

typedef struct _GtkFloatingContainer GtkFloatingContainer;
typedef struct _GtkFloatingContainerClass GtkFloatingContainerClass;
typedef struct _GtkFloatingContainerChild GtkFloatingContainerChild;

struct _GtkFloatingContainer {
  // Parent class.
  GtkBin bin;

  // A GList of all our floating children, in GtkFloatingContainerChild
  // structs. Owned by the GtkFloatingContainer.
  GList* floating_children;
};

struct _GtkFloatingContainerClass {
  GtkBinClass parent_class;
};

// Internal structure used to associate a widget and its x/y child properties.
struct _GtkFloatingContainerChild {
  GtkWidget* widget;
  gint x;
  gint y;
};

GType      gtk_floating_container_get_type() G_GNUC_CONST;
GtkWidget* gtk_floating_container_new();
void       gtk_floating_container_add_floating(GtkFloatingContainer* container,
                                               GtkWidget* widget);
// Use gtk_container_remove to remove all widgets; both widgets added with
// gtk_container_add() and gtk_floating_container_add_floating().

G_END_DECLS

#endif  // CHROME_BROWSER_GTK_GTK_FLOATING_CONTAINER_H_
