// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StateTrackingString_h
#define StateTrackingString_h

#include "config.h"
#include "ChromiumBridge.h"
#include "PlatformString.h"

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
      ChromiumBridge::NotifyFormStateChanged(m_parentNode->ownerDocument());
    }

    // The node that owns this value.
    Node* m_parentNode;

    // Actual data of this string.
    String m_string;
};

}  // namespace WebCore

#endif  // StateTrackingString_h

