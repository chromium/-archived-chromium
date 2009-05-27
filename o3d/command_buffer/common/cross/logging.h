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


// This file abstracts differences in logging between NaCl and host
// environment.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_LOGGING_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_LOGGING_H_

#ifndef __native_client__
#include "base/logging.h"
#else
#include <sstream>

// TODO: implement logging through nacl's debug service runtime if
// available.
#define CHECK(X) do {} while (0)
#define CHECK_EQ(X, Y) do {} while (0)
#define CHECK_NE(X, Y) do {} while (0)
#define CHECK_GT(X, Y) do {} while (0)
#define CHECK_GE(X, Y) do {} while (0)
#define CHECK_LT(X, Y) do {} while (0)
#define CHECK_LE(X, Y) do {} while (0)

#define DCHECK(X) do {} while (0)
#define DCHECK_EQ(X, Y) do {} while (0)
#define DCHECK_NE(X, Y) do {} while (0)
#define DCHECK_GT(X, Y) do {} while (0)
#define DCHECK_GE(X, Y) do {} while (0)
#define DCHECK_LT(X, Y) do {} while (0)
#define DCHECK_LE(X, Y) do {} while (0)

#define LOG(LEVEL) if (0) std::ostringstream()
#define DLOG(LEVEL) if (0) std::ostringstream()

#endif

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_LOGGING_H_
