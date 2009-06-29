/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_PLUGIN_MAC_PLUGIN_MAC_H_
#define O3D_PLUGIN_MAC_PLUGIN_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <npupp.h>
#include <AGL/agl.h>
#include <vector>

// Just for o3d::Event::Button at the moment.
#include "core/cross/event.h"


// RenderTimer maintains an animation timer (nominally running at 60fps)
//
// Keeps track of the current NPP instances running in the browser and then
// renders each one during each timer callback.
class RenderTimer {
 public:
  RenderTimer() {}

  void Start();
  void Stop();

  void AddInstance(NPP instance);
  void RemoveInstance(NPP instance);

  typedef std::vector<NPP>::iterator InstanceIterator;

 private:
  RenderTimer(const RenderTimer&);

  static void TimerCallback(CFRunLoopTimerRef timer, void* info);
  CFRunLoopTimerRef timerRef_;
  static std::vector<NPP> instances_;
};

extern RenderTimer gRenderTimer;

void InitializeBreakpad();
void ShutdownBreakpad();


void* SafariBrowserWindowForWindowRef(WindowRef theWindow);

void* SelectedTabForSafariBrowserWindow(void* cocoaWindow);

void ReleaseSafariBrowserWindow(void* browserWindow);


// Some miscellaneous helper functions...


void CFReleaseIfNotNull(CFTypeRef cf);


// Converts an old style Mac HFS path eg "HD:Users:xxx:file.zip" into a standard
// Posix path eg "/Users/xxx/file.zip" Assumes UTF8 in and out, returns a block
// of memory allocated with new, so you'll want to delete this at some point.
// Returns NULL in the event of an error.
char* CreatePosixFilePathFromHFSFilePath(const char* hfsPath);

bool HandleMacEvent(EventRecord* the_event, NPP instance);

o3d::Event::Button MacOSMouseButtonNumberToO3DButton(int inButton);

bool GetBrowserVersionInfo(int *returned_major,
                           int *returned_minor,
                           int *returned_bugfix);

bool UseSoftwareRenderer();

#endif  // O3D_PLUGIN_MAC_PLUGIN_MAC_H_
