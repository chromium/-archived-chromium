/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "SharedTimer.h"
#include "SystemTime.h"
#include "Assertions.h"

#undef LOG

#include "base/message_loop.h"

namespace WebCore {

class WebkitTimerTask;

// We maintain a single active timer and a single active task for
// setting timers directly on the platform.
static WebkitTimerTask* msgLoopTask;
static void (*sharedTimerFiredFunction)();

// Timer task to run in the chrome message loop.
class WebkitTimerTask : public Task {
public:
    WebkitTimerTask(void (*callback)()) : m_callback(callback) {}

    virtual void Run() {
        if (!m_callback)
            return;
        // Since we only have one task running at a time, verify that it
        // is 'this'.
        ASSERT(msgLoopTask == this);
        msgLoopTask = NULL;
        m_callback();
    }

    void Cancel() {
        m_callback = NULL;
    }

private:
    void (*m_callback)();

    DISALLOW_COPY_AND_ASSIGN(WebkitTimerTask);
};

void setSharedTimerFiredFunction(void (*f)())
{
    sharedTimerFiredFunction = f;
}

void setSharedTimerFireTime(double fireTime)
{
    ASSERT(sharedTimerFiredFunction);
    int interval = static_cast<int>( (fireTime - currentTime()) * 1000);
    if (interval < 0)
        interval = 0;

    stopSharedTimer();

    // Verify that we didn't leak the task or timer objects.
    ASSERT(msgLoopTask == NULL);

    msgLoopTask = new WebkitTimerTask(sharedTimerFiredFunction);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, msgLoopTask, interval);
}

void stopSharedTimer()
{
    if (!msgLoopTask)
        return;

    msgLoopTask->Cancel();
    msgLoopTask = NULL;
}

} // namespace WebCore
