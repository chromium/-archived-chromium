/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "base/compiler_specific.h"

#define WIN32_COMPILE_HACK

MSVC_PUSH_WARNING_LEVEL(0);
#include "Color.h"
#include "SSLKeyGenerator.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
MSVC_POP_WARNING();

using namespace WebCore;

Color WebCore::focusRingColor() { notImplemented(); return 0xFF7DADD9; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { notImplemented(); }

String WebCore::signedPublicKeyAndChallengeString(unsigned, const String&, const KURL&) { notImplemented(); return String(); }

String KURL::fileSystemPath() const { notImplemented(); return String(); }

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    notImplemented();
    return 0;
}

namespace WTF {
void scheduleDispatchFunctionsOnMainThread() { notImplemented(); }
}

#if USE(JSC)
#include "EventLoop.h"
#include "PluginView.h"

void EventLoop::cycle() { notImplemented(); }

void PluginView::setJavaScriptPaused(bool) { notImplemented(); }
#endif
