// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ChromeClientWin_h
#define ChromeClientWin_h

#include <wtf/Forward.h>

#include "ChromeClient.h"

namespace WebCore {

    class FileChooser;
    class Frame;
    class String;
    
    class ChromeClientChromium : public ChromeClient {
    public:
        // Opens the file selection dialog.
        virtual void runFileChooser(const String& defaultFileName,
                                    PassRefPtr<FileChooser> file_chooser) = 0;

        // Given a rect in main frame coordinates, returns a new rect relative
        // to the screen.
        virtual IntRect windowToScreen(const IntRect& rect) = 0;
    };
}

#endif // ChromeClientWin_h

