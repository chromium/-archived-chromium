// Copyright (c) 2009, Google Inc.
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

#include "config.h"

#include "Threading.h"
#undef LOG

#include "base/message_loop.h"
#include "base/task.h"

namespace WTF {

struct NewThreadContext {
    NewThreadContext(ThreadFunction entryPoint, void* data, const char* name)
        : entryPoint(entryPoint)
        , data(data)
        , name(name)
    { }

    ThreadFunction entryPoint;
    void* data;
    std::string name;

    Mutex creationMutex;
};

static void* threadEntryPoint(void* contextData)
{
    NewThreadContext* context = reinterpret_cast<NewThreadContext*>(contextData);

    // Block until our creating thread has completed any extra setup work
    {
        MutexLocker locker(context->creationMutex);
    }

    // Run via Chrome's message loop
    MessageLoop message_loop;
    message_loop.set_thread_name(context->name);
    message_loop.PostTask(FROM_HERE,
                          NewRunnableFunction(context->entryPoint, context->data));
    message_loop.Run();

    delete context;

    return NULL;
}

ThreadIdentifier createThread(ThreadFunction entryPoint, void* data, const char* name)
{
    NewThreadContext* context = new NewThreadContext(entryPoint, data, name);

    // Prevent the thread body from executing until we've established the thread identifier
    MutexLocker locker(context->creationMutex);

    return createThreadInternal(threadEntryPoint, context, name);
}

#if PLATFORM(MAC) || PLATFORM(WIN)

// This function is deprecated but needs to be kept around for backward
// compatibility. Use the 3-argument version of createThread above.

ThreadIdentifier createThread(ThreadFunction entryPoint, void* data)
{
    return createThread(entryPoint, data, 0);
}
#endif

} // namespace WTF
