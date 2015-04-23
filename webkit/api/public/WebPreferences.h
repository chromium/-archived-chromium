/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebPreferences_h
#define WebPreferences_h

#error "This header file is still a work in progress; do not include!"

#include "WebCommon.h"

namespace WebKit {
    class WebString;

    class WebPreferences {
    public:
        enum BooleanKey {
            DeveloperExtrasEnabled,
            DOMPasteEnabled,
            JavaEnabled,
            JavaScriptEnabled,
            JavaScriptCanOpenWindowsAutomatically,
            JavaScriptCanCloseWindowsAutomatically,
            LoadsImagesAutomatically,
            PluginsEnabled,
            ShrinksStandaloneImagesToFit,
            TabKeyCyclesThroughElements,
            TextAreasAreResizable,
            UserStyleSheetEnabled,
            UsesPageCache,
            UsesUniversalDetector,
        };

        enum IntegerKey {
            DefaultFixedFontSize,
            DefaultFontSize,
            MinimumFontSize,
            MinimumLogicalFontSize,
        };

        enum StringKey {
            CursiveFontFamily,
            DefaultEncoding,
            FantasyFontFamily,
            FixedFontFamily,
            SansSerifFontFamily,
            SerifFontFamily,
            StandardFontFamily,
        };

        enum URLKey {
            UserStyleSheetLocation,
        };

        virtual bool getValue(BooleanKey) const = 0;
        virtual void setValue(BooleanKey, bool) = 0;

        virtual int getValue(IntegerKey) const = 0;
        virtual void setValue(IntegerKey, int) = 0;

        virtual WebString getValue(StringKey) const = 0;
        virtual void setValue(StringKey, const WebString&) = 0;

        virtual WebURL getValue(URLKey) const = 0;
        virtual void setValue(URLKey, const WebURL&) = 0;
    };

} // namespace WebKit
