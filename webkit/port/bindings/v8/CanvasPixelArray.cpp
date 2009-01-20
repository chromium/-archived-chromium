/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
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

#include "config.h"
#include <CanvasPixelArray.h>

#include <v8_proxy.h>
#include <wtf/Assertions.h>

namespace WebCore {

    CanvasPixelArray::CanvasPixelArray(size_t length)
    {
        ASSERT((length & 3) == 0);  // Length must be a multiple of 4.
        m_length = length;
        m_data = static_cast<unsigned char*>(fastMalloc(m_length));
    }

    CanvasPixelArray::~CanvasPixelArray()
    {
        fastFree(m_data);
        // Ensure that accesses after destruction will fail. In case there is a
        // dangling reference.
        m_length = 0;
        m_data = reinterpret_cast<unsigned char*>(-1);
    }

    size_t CanvasPixelArray::length() const
    {
        return m_length;
    }

    unsigned char* CanvasPixelArray::data()
    {
        return m_data;
    }

    unsigned char CanvasPixelArray::get(size_t index)
    {
        ASSERT(index < m_length);
        return m_data[index];
    }

    void CanvasPixelArray::set(size_t index, double value)
    {
        ASSERT(index < m_length);
        if (!(value > 0)) {  // Test for NaN and less than zero in one go.
            value = 0;
        } else if (value > 255) {
            value = 255;
        }
        m_data[index] = static_cast<unsigned char>(value + 0.5);
    }

}  // namespace WebCore
