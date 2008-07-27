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

#include "base/timer.h"
#include "base/message_loop.h"
#include <map>

namespace WebCore {

class WebkitTimerTask;

// We maintain a single active timer and a single active task for
// setting timers directly on the platform.
static Timer* msgLoopTimer;
static WebkitTimerTask* msgLoopTask;
static void (*sharedTimerFiredFunction)();

// Timer task to run in the chrome message loop.
class WebkitTimerTask : public Task {
public:
    WebkitTimerTask(void (*callback)()) : m_callback(callback) {}
    virtual void Run() {
        MessageLoop* msgloop = MessageLoop::current();
        delete msgLoopTimer;
        msgLoopTimer = NULL;
        // Since we only have one task running at a time, verify that it
        // is 'this'.
        ASSERT(msgLoopTask == this);
        msgLoopTask = NULL;
        m_callback();
        delete this;
    }

private:
    void (*m_callback)();

    DISALLOW_EVIL_CONSTRUCTORS(WebkitTimerTask);
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

    MessageLoop* msgloop = MessageLoop::current();
    TimerManager* mgr = msgloop->timer_manager();

    // Verify that we didn't leak the task or timer objects.
    ASSERT(msgLoopTimer == NULL);
    ASSERT(msgLoopTask == NULL);

    // Create a new task and timer.  We are responsible for cleanup 
    // of each.
    msgLoopTask = new WebkitTimerTask(sharedTimerFiredFunction);
    msgLoopTimer = mgr->StartTimer(interval, msgLoopTask, false);
}

void stopSharedTimer()
{
    if (msgLoopTimer != NULL) {
        // When we have a timer running, there must be an associated task.
        ASSERT(msgLoopTask != NULL);

        MessageLoop* msgloop = MessageLoop::current();
        // msgloop can be NULL in one particular instance:
        // KJS uses static GCController object, which has a Timer member,
        // which attempts to stop() when it's destroyed, which calls this.
        // But since the object is static, and the current MessageLoop can be
        // scoped to main(), the static object can be destroyed (and this
        // code can run) after the current MessageLoop is gone.
        // TODO(evanm): look into whether there's a better solution for this.
        if (msgloop) {
            TimerManager* mgr = msgloop->timer_manager();
            mgr->StopTimer(msgLoopTimer);
        }

        delete msgLoopTimer;
        msgLoopTimer = NULL;
        delete msgLoopTask;
        msgLoopTask = NULL;
    }
}

}
