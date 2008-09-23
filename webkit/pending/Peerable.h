// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Peerable_h
#define Peerable_h

#include <wtf/Noncopyable.h>

#if USE(V8_BINDING)

namespace WebCore {

class Peerable : Noncopyable {
public:
    virtual void setPeer(void* peer) = 0;
    virtual void* peer() const = 0;
protected:
    virtual ~Peerable() { }
};

};

#endif  // USE(V8_BINDING)
#endif  // Peerable_h
