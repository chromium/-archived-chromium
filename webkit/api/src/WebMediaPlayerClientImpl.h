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

#ifndef WebMediaPlayerClientImpl_h
#define WebMediaPlayerClientImpl_h

#if ENABLE(VIDEO)

#include "WebMediaPlayerClient.h"

#include "MediaPlayerPrivate.h"
#include "WebCanvas.h"
#include <wtf/OwnPtr.h>

namespace WebKit {
    class WebMediaPlayer;

    // This class serves as a bridge between WebCore::MediaPlayer and
    // WebKit::WebMediaPlayer.
    class WebMediaPlayerClientImpl : public WebMediaPlayerClient
                                   , public WebCore::MediaPlayerPrivateInterface {
    public:
        static void setIsEnabled(bool);
        static void registerSelf(WebCore::MediaEngineRegistrar);

        // WebMediaPlayerClient methods:
        virtual void networkStateChanged();
        virtual void readyStateChanged();
        virtual void volumeChanged();
        virtual void timeChanged();
        virtual void repaint();
        virtual void durationChanged();
        virtual void rateChanged();
        virtual void sizeChanged();
        virtual void sawUnsupportedTracks();

        // MediaPlayerPrivateInterface methods:
        virtual void load(const WebCore::String& url);
        virtual void cancelLoad();
        virtual void play();
        virtual void pause();
        virtual WebCore::IntSize naturalSize() const;
        virtual bool hasVideo() const;
        virtual void setVisible(bool);
        virtual float duration() const;
        virtual float currentTime() const;
        virtual void seek(float time);
        virtual bool seeking() const;
        virtual void setEndTime(float time);
        virtual void setRate(float);
        virtual bool paused() const;
        virtual void setVolume(float);
        virtual WebCore::MediaPlayer::NetworkState networkState() const;
        virtual WebCore::MediaPlayer::ReadyState readyState() const;
        virtual float maxTimeSeekable() const;
        virtual float maxTimeBuffered() const;
        virtual int dataRate() const;
        virtual void setAutobuffer(bool);
        virtual bool totalBytesKnown() const;
        virtual unsigned totalBytes() const;
        virtual unsigned bytesLoaded() const;
        virtual void setSize(const WebCore::IntSize&);
        virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);

    private:
        WebMediaPlayerClientImpl();

        static WebCore::MediaPlayerPrivateInterface* create(WebCore::MediaPlayer*);
        static void getSupportedTypes(WTF::HashSet<WebCore::String>&);
        static WebCore::MediaPlayer::SupportsType supportsType(
            const WebCore::String& type, const WebCore::String& codecs);

        WebCore::MediaPlayer* m_mediaPlayer;
        OwnPtr<WebMediaPlayer> m_webMediaPlayer;
        OwnPtr<WebKit::WebCanvas> m_webCanvas;
    };

} // namespace WebKit

#endif

#endif
