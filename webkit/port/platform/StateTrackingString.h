// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#ifndef StateTrackingString_h
#define StateTrackingString_h

#include "config.h"
#include "String.h"

#include "webkit/glue/webkit_glue.h"

namespace WebCore {

// This string class emulates the necessary calls that the form code makes
// and notifies the embedder if the value changes. We do not derive from
// String so that if we don't implement some function that a WebKit merge uses,
// the build will break instead of us silently missing that set.
class StateTrackingString {
public:
    StateTrackingString(Node* parentNode) : m_parentNode(parentNode) {
        Notify();
    }
    ~StateTrackingString() {
      // It would be nice to notify here, but when the string is going away,
      // that means the input element is going away, and we can't get at its
      // information.
    }

    void operator=(const StateTrackingString& other) {
        // Don't copy m_parentNode. StateTrackingString should keep its
        // parent node in its life time. The other's parent node may be 
        // deleted.
        m_string = other.m_string;
    }
      
    void operator=(const char* other) {
        m_string = other;
        Notify();
    }
    void operator=(const String& other) {
        m_string = other;
        Notify();
    }

    // Things that don't need interception.
    bool isNull() const {
        return m_string.isNull();
    }
    unsigned length() const {
        return m_string.length();
    }

    // It can be magically converted to a string reference when passed to other
    // functions.
    operator const String&() const { return m_string; }

private:
    void Notify() const {
      webkit_glue::NotifyFormStateChanged(m_parentNode->ownerDocument());
    }

    // The node that owns this value.
    Node* m_parentNode;

    // Actual data of this string.
    String m_string;
};

}  // namespace WebCore

#endif  // StateTrackingString_h
