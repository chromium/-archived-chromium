// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef FramelessScrollViewClient_h
#define FramelessScrollViewClient_h

#include "HostWindow.h"

namespace WebCore {
    class FramelessScrollViewClient : public HostWindow {
    public:
        virtual void popupClosed(FramelessScrollView* popup_view) = 0;
    };
}

#endif // FramelessScrollViewClient_h
