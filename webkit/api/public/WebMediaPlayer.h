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

#ifndef WebMediaPlayer_h
#define WebMediaPlayer_h

#include "WebCanvas.h"

namespace WebKit {
    class WebMediaPlayerClient;
    class WebURL;
    struct WebRect;
    struct WebSize;

    class WebMediaPlayer {
    public:
        enum NetworkState {
            Empty,
            Idle,
            Loading,
            Loaded,
            FormatError,
            NetworkError,
            DecodeError,
        };

        enum ReadyState {
            HaveNothing,
            HaveMetadata,
            HaveCurrentData,
            HaveFutureData,
            HaveEnoughData,
        };

        virtual ~WebMediaPlayer() {}

        virtual void load(const WebURL&) = 0;
        virtual void cancelLoad() = 0;

        // Playback controls.
        virtual void play() = 0;
        virtual void pause() = 0;
        virtual void seek(float seconds) = 0;
        virtual void setEndTime(float seconds) = 0;
        virtual void setRate(float) = 0;
        virtual void setVolume(float) = 0;
        virtual void setVisible(bool) = 0;
        virtual bool setAutoBuffer(bool) = 0;
        virtual bool totalBytesKnown() = 0;
        virtual float maxTimeBuffered() const = 0;
        virtual float maxTimeSeekable() const = 0;

        virtual void setSize(const WebSize&) = 0;

        virtual void paint(WebCanvas*, const WebRect&) = 0;

        // True if a video is loaded.
        virtual bool hasVideo() const = 0;

        // Dimension of the video.
        virtual WebSize naturalSize() const = 0;

        // Getters of playback state.
        virtual bool paused() const = 0;
        virtual bool seeking() const = 0;
        virtual float duration() const = 0;
        virtual float currentTime() const = 0;

        // Get rate of loading the resource.
        virtual int dataRate() const = 0;

        // Internal states of loading and network.
        virtual NetworkState networkState() const = 0;
        virtual ReadyState readyState() const = 0;

        virtual unsigned long long bytesLoaded() const = 0;
        virtual unsigned long long totalBytes() const = 0;
    };

} // namespace WebKit

#endif
