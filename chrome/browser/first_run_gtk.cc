// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "chrome/app/breakpad_linux.h"
// We need to reach through the browser process to tweak the metrics flag.
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/installer/util/google_update_settings.h"

#include "base/message_loop.h"

namespace {

// Callback for the "response" signal of the first run dialog.
// Fills in the int* |data| with the dialog response and quits the message loop.
// See the TODO below for why this is necessary (it's a bug).
void DialogResponseCallback(GtkDialog* dialog, gint response,
                            gpointer data) {
  int* response_out = static_cast<int*>(data);
  *response_out = response;
  MessageLoop::current()->Quit();
}

}  // namespace

bool OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton) {
#if defined(GOOGLE_CHROME_BUILD)
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      "Google Chrome Dev Build",
      NULL,  // No parent
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  g_signal_connect(G_OBJECT(dialog), "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), 18);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 12);
  // Force a size on the vbox so the labels wrap.
  gtk_widget_set_size_request(vbox, 400, -1);

  GtkWidget* privacy_label = gtk_label_new(
      "This version of Google Chrome for Linux is not appropriate for "
      "general consumer use.  Certain privacy features are unavailable "
      "at this time as described in our privacy policy at");
  gtk_misc_set_alignment(GTK_MISC(privacy_label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(privacy_label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), privacy_label, FALSE, FALSE, 0);

  GtkWidget* url_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(url_label),
      "<tt>http://www.google.com/chrome/intl/en/privacy_linux.html</tt>");
  // Set selectable to allow copy and paste.
  gtk_label_set_selectable(GTK_LABEL(url_label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), url_label, FALSE, FALSE, 0);

  GtkWidget* intro_label = gtk_label_new(
      "This dialog would normally prompt you to import information from other "
      "browsers, but that is not yet fully implemented.\n"
      "Instead, we have only one important setting available: Crash dumps. "
      "We cannot fix your crashes without your crash reports, so there's "
      "little reason to run a dev channel build without turning them on.");
  gtk_misc_set_alignment(GTK_MISC(intro_label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(intro_label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), intro_label, FALSE, FALSE, 0);

  GtkWidget* check = gtk_check_button_new();
  GtkWidget* check_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(check_label),
                       "<b>Optional:</b> Help make Google Chrome better by "
                       "automatically sending crash reports (and eventually "
                       "usage statistics, but that is also unimplemented) "
                       "to Google.");
  gtk_label_set_line_wrap(GTK_LABEL(check_label), TRUE);
  gtk_container_add(GTK_CONTAINER(check), check_label);
  gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);

  #define UTF8_BULLET " \xE2\x80\xA2 "
  GtkWidget* crashinfo_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(crashinfo_label),
      "A crash dump contains:\n"
      UTF8_BULLET "Stacks and registers of all the threads in the crashing "
      "process\n"
      UTF8_BULLET "The current URL if a render process crashes\n"
      UTF8_BULLET "<tt>/proc/cpuinfo</tt>, <tt>/etc/lsb-release</tt>\n"
      UTF8_BULLET "Other misc information about the process (its "
      "<tt>/proc/pid/maps</tt>, <tt>/proc/pid/status</tt>, etc.)");
  gtk_misc_set_alignment(GTK_MISC(crashinfo_label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(crashinfo_label), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), crashinfo_label, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(content_area), vbox, FALSE, FALSE, 0);
  gtk_widget_show_all(vbox);

  // TODO(port): it should be sufficient to just run the dialog:
  // int response = gtk_dialog_run(GTK_DIALOG(dialog));
  // but that spins a nested message loop and hoses us.  :(
  // http://code.google.com/p/chromium/issues/detail?id=12552
  // Instead, run a loop and extract the response manually.
  int response = 0;
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(DialogResponseCallback), &response);
  gtk_widget_show(dialog);
  MessageLoop::current()->Run();
  // End of above TODO.

  bool accepted = (response == GTK_RESPONSE_ACCEPT);
  if (accepted) {
    // Mark that first run has ran.
    FirstRun::CreateSentinel();
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check))) {
      // They opted in.
      if (GoogleUpdateSettings::SetCollectStatsConsent(true)) {
        InitCrashReporter();
      }
    } else {
      GoogleUpdateSettings::SetCollectStatsConsent(false);
    }
  }

  gtk_widget_destroy(dialog);
  return accepted;
#else  // defined(GOOGLE_CHROME_BUILD)
  FirstRun::CreateSentinel();
  return true;
#endif
}
