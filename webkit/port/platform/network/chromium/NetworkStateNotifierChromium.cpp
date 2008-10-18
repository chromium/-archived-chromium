/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "NetworkStateNotifier.h"

namespace WebCore {

// Chromium doesn't currently support network state notifications. This causes
// an extra DLL to get loaded into the renderer which can slow things down a
// bit. We may want an alternate design.

void NetworkStateNotifier::updateState()
{
}

// TODO(darin): Kill this once we stop defining PLATFORM(MAC)
#if PLATFORM(MAC)
void NetworkStateNotifier::networkStateChangeTimerFired(Timer<NetworkStateNotifier>*)
{
}
#endif

#if PLATFORM(WIN_OS) || PLATFORM(MAC)
NetworkStateNotifier::NetworkStateNotifier()
    : m_isOnLine(true)
#if PLATFORM(MAC)
    , m_networkStateChangeTimer(this, &NetworkStateNotifier::networkStateChangeTimerFired)
#endif
{
}
#endif

}
