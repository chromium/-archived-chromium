// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(VIDEO)

#include "MediaPlayerPrivateChromium.h"

namespace WebCore {

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::DataUnavailable)
{
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
}

IntSize MediaPlayerPrivate::naturalSize() const
{
  return IntSize(0, 0);
}

bool MediaPlayerPrivate::hasVideo() const
{
  return false;
}

void MediaPlayerPrivate::load(const String& url)
{
    // Always fail for now
    m_networkState = MediaPlayer::LoadFailed;
    m_readyState = MediaPlayer::DataUnavailable;
    m_player->networkStateChanged();
    m_player->readyStateChanged();
}

void MediaPlayerPrivate::cancelLoad()
{
}

void MediaPlayerPrivate::play()
{
}

void MediaPlayerPrivate::pause()
{
}

bool MediaPlayerPrivate::paused() const
{
    return true;
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

float MediaPlayerPrivate::duration() const
{
    return 0.0f;
}

float MediaPlayerPrivate::currentTime() const
{
    return 0.0f;
}

void MediaPlayerPrivate::seek(float time)
{
}

void MediaPlayerPrivate::setEndTime(float)
{
}

void MediaPlayerPrivate::setRate(float)
{
}

void MediaPlayerPrivate::setVolume(float)
{
}

int MediaPlayerPrivate::dataRate() const
{
    return 0;
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

float MediaPlayerPrivate::maxTimeBuffered() const
{
    return 0.0f;
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    return 0.0f;
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    return 0;
}

bool MediaPlayerPrivate::totalBytesKnown() const
{
    return false;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    return 0;
}

void MediaPlayerPrivate::setVisible(bool)
{
}

void MediaPlayerPrivate::setRect(const IntRect&)
{
}

void MediaPlayerPrivate::loadStateChanged()
{
}

void MediaPlayerPrivate::didEnd()
{
}

void MediaPlayerPrivate::paint(GraphicsContext* p, const IntRect& r)
{
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    // We support nothing right now!
}

bool MediaPlayerPrivate::isAvailable()
{
    // Must return true in order to build HTMLMedia/Video/AudioElements,
    // otherwise WebKit will replace the tags with an empty tag
    return true;
}

}

#endif
