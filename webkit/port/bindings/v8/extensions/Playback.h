// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The IntervalExtension is a v8 extension to implement a simple interval
// class for measuring microsecond intervals.

#ifndef PLAYBACK_EXTENSION_H__
#define PLAYBACK_EXTENSION_H__

#include "v8.h"

namespace WebCore {

// Inject code which overrides a few common JS functions for implementing
// randomness.  In order to implement effective record & playback of
// websites, it is important that the URLs not change.  Many popular web
// based apps use randomness in URLs to unique-ify urls for proxies.
// Unfortunately, this breaks playback.
// To work around this, we take the two most common client-side randomness
// generators and make them constant.  They really need to be constant
// (rather than a constant seed followed by constant change)
// because the playback mode wants flexibility in how it plays them back
// and cannot always guarantee that requests for randomness are played back
// in exactly the same order in which they were recorded.
class PlaybackExtension {
 public:
  static v8::Extension* Get();
};

}

#endif  // PLAYBACK_EXTENSION_H__
