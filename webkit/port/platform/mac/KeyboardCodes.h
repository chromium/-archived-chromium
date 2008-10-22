// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef KeyboardCodes_h
#define KeyboardCodes_h

// Virtual key codes on the Mac haven't been documented since the good old
// days.  Seriously.  The most recent documentation is from 1992, in the
// Classic Mac OS days.  Fortunately, the "extended keyboard" key codes
// published then are the same virtual key codes used by Mac OS X today.
//
// Source: Inside Macintosh: Macintosh Toolbox Essentials (IM:Tb)
// Chapter 2 - Event Manager: Using the Event Manager: Handling Low-Level
// Events: Responding to Keyboard Events
// Figure 2-10: Virtual key codes for the Apple Extended Keyboard II
// HTML (with a low-ish-resolution image):
//   http://developer.apple.com/documentation/mac/Toolbox/Toolbox-40.html#MARKER-9-184
// PDF (with a legible vector image, page 2-43):
//   http://developer.apple.com/documentation/mac/pdf/MacintoshToolboxEssentials.pdf

// TODO(port): Don't reuse the names used by Windows.  Pick new names and use
// them here, and provide a Windows version of KeyboardCodes.h that defines
// the new constants in terms of the VK_* names.
enum {
  VK_RETURN = 0x24,
  VK_ESCAPE = 0x35,
  VK_HOME = 0x73,
  VK_PRIOR = 0x74,  // Page Up
  VK_END = 0x77,
  VK_NEXT = 0x79,  // Page Down
  VK_DOWN = 0x7D,
  VK_UP = 0x7E,
};

#endif // KeyboardCodes_h
