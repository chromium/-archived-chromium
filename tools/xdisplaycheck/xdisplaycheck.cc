// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a small program that tries to connect to the X server.  It
// continually retries until it connects or 5 seconds pass.  If it fails
// to connect to the X server after 5 seconds, it returns an error code
// of -1.
//
// This is to help verify that the X server is available before we start
// start running tests on the build bots.

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <X11/Xlib.h>

void Sleep(int duration_ms) {
  struct timespec sleep_time, remaining;

  // Contains the portion of duration_ms >= 1 sec.
  sleep_time.tv_sec = duration_ms / 1000;
  duration_ms -= sleep_time.tv_sec * 1000;

  // Contains the portion of duration_ms < 1 sec.
  sleep_time.tv_nsec = duration_ms * 1000 * 1000;  // nanoseconds.

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

int main(int argc, char* argv[]) {
  int kNumTries = 50;
  for (int i = 0; i < kNumTries; ++i) {
    Display* display = XOpenDisplay(NULL);
    if (display)
      return 0;
    Sleep(100);
  }

  printf("Failed to connect to %s\n", XDisplayName(NULL));
  return -1;
}
