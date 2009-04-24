// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_ABOUT_CHROME_DIALOG_H_
#define CHROME_BROWSER_GTK_ABOUT_CHROME_DIALOG_H_

class Profile;
typedef struct _GtkWindow GtkWindow;

// Displays the about box, using data copied from |profile|.
void ShowAboutDialogForProfile(GtkWindow* parent, Profile* profile);

#endif  // CHROME_BROWSER_GTK_ABOUT_CHROME_DIALOG_H_
