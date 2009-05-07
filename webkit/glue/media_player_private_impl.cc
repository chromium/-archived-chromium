// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(VIDEO)

#include "MediaPlayerPrivateChromium.h"

#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayerclient_impl.h"

namespace WebCore {

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar) {
  if (webkit_glue::IsMediaPlayerAvailable()) {
    registrar(&WebMediaPlayerClientImpl::create,
              &WebMediaPlayerClientImpl::getSupportedTypes,
              &WebMediaPlayerClientImpl::supportsType);
  }
}

}  // namespace WebCore

#endif
