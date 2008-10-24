// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "config.h"
#import "Pasteboard.h"

#import "DocumentFragment.h"
#import "NotImplemented.h"

/*
    A stub for a X11 pasteboard
   */
namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    notImplemented();
    return 0;
}

void Pasteboard::clear()
{
    notImplemented();
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    notImplemented();
}

void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame)
{
    notImplemented();
}

void Pasteboard::writeImage(Node* node, const KURL& url, const String& title)
{
    notImplemented();
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

String Pasteboard::plainText(Frame* frame)
{
    notImplemented();
    return String();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    notImplemented();
    return 0;
}

}
