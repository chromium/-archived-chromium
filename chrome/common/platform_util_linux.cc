// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#include <gtk/gtk.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/string_util.h"

namespace {

void XDGOpen(const FilePath& path) {
  std::vector<std::string> argv;
  argv.push_back("xdg-open");
  argv.push_back(path.value());
  base::file_handle_mapping_vector no_files;
  base::LaunchApp(argv, no_files, true, NULL);
}

}  // namespace

namespace platform_util {

// TODO(estade): It would be nice to be able to select the file in the file
// manager, but that probably requires extending xdg-open. For now just
// show the folder.
void ShowItemInFolder(const FilePath& full_path) {
  FilePath dir = full_path.DirName();
  if (!file_util::DirectoryExists(dir))
    return;

  XDGOpen(dir);
}

void OpenItem(const FilePath& full_path) {
  XDGOpen(full_path);
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  // A detached widget won't have a toplevel window as an ancestor, so we can't
  // assume that the query for toplevel will return a window.
  GtkWidget* toplevel = gtk_widget_get_ancestor(view, GTK_TYPE_WINDOW);
  return GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
}

string16 GetWindowTitle(gfx::NativeWindow window) {
  const gchar* title = gtk_window_get_title(window);
  return UTF8ToUTF16(title);
}

bool IsWindowActive(gfx::NativeWindow window) {
  return gtk_window_is_active(window);
}

bool IsVisible(gfx::NativeView view) {
  return GTK_WIDGET_VISIBLE(view);
}

}  // namespace platform_util
