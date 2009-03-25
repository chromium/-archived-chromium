// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

/* Small program to dump the contents of GTK's clipboard to the terminal.
 * Feel free to add to it or improve formatting or whatnot.
 */
int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);
  GdkAtom* targets;
  int num_targets = 0;
  // For now only look at GDK_SELECTION_CLIPBOARD. This could be extended
  // to look at GDK_SELECTION_PRIMARY (the X clipboard).
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

  gtk_clipboard_wait_for_targets(clip, &targets, &num_targets);

  printf("Available targets:\n---------------\n");

  for (int i = 0; i < num_targets; i++) {
    printf("  [format: %s", gdk_atom_name(targets[i]));
    GtkSelectionData* data = gtk_clipboard_wait_for_contents(clip, targets[i]);
    if (!data) {
      printf("]: NULL\n\n");
      continue;
    }

    printf(" / length: %d / bits %d]: ", data->length, data->format);

    if (strstr(gdk_atom_name(targets[i]), "image")) {
      printf("(image omitted)\n\n");
      continue;
    }

    for (int j = 0; j < data->length; j++) {
      // Output data one byte at a time. Currently wide strings look
      // pretty weird.
      printf("%c", (data->data[j] == 0 ? '_' : data->data[j]));
    }
    printf("\n\n");
  }
}
