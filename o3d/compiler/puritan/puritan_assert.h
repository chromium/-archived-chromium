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


#ifndef PURITAN_ASSERT_H
#define PURITAN_ASSERT_H

#include <string>
#include <sstream>

#ifndef __GNUC__

// MSVC and GCC are different - MSVC needs assert worker to be at the top level
// for boost to compile.  GCC does not.  I don't know which is correct

__declspec (noreturn) void puritan_assert_worker (const char *, 
                                                  const char *, 
                                                  unsigned, 
                                                  const  std::string &);
namespace Salem { namespace Puritan {
#else
namespace Salem { namespace Puritan {

void puritan_assert_worker (const char *, 
                            const char *, 
                            unsigned, 
                            const  std::string &) __attribute__((noreturn));
    
#endif


#define PURITAN_ABORT(message)                              \
do {                                                        \
        std::ostringstream message_buf;                     \
        message_buf << message;                             \
        puritan_assert_worker (__FILE__,                    \
                               __FUNCTION__,                \
                               __LINE__,                    \
                               message_buf.str());          \
} while (0)

#define PURITAN_ASSERT(check, message)          \
do {                                            \
    if (!(check))                               \
    {                                           \
        PURITAN_ABORT(message);                 \
    }                                           \
} while (0)

#undef assert
#define assert(x) PURITAN_ASSERT(x,"")
}
}

#endif
