// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "IntRect.h"
#include "MediaPlayerPrivateChromium.h"
#include "PlatformContextSkia.h"
#undef LOG

#include "googleurl/src/gurl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/webmediaplayer_delegate.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webview_delegate.h"

namespace WebCore {

// We can't create the delegate here because m_player->frameView is null at
// this moment. Although we can static_cast the MediaPlayerClient to 
// HTMLElement and get the frame from there, but creating the delegate from 
// load() seems to be a better idea.
MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player),
      m_delegate(NULL) {
}

MediaPlayerPrivate::~MediaPlayerPrivate() {
  // Delete the delegate, it should delete the associated WebMediaPlayer.
  delete m_delegate;
}

void MediaPlayerPrivate::load(const String& url) {
  // Delete the delegate if it already exists. Because we may be in a different
  // view since last load. WebMediaPlayer uses the view internally when
  // using ResourceHandle, WebMediaPlayerDelegate contains the actual media
  // player, in order to hook the actual media player with ResourceHandle in
  // WebMediaPlayer given the new view, we destroy the old
  // WebMediaPlayerDelegate and WebMediaPlayer and create a new set of both.
  delete m_delegate;
  m_delegate = NULL;

  webkit_glue::WebMediaPlayer* media_player = 
      new webkit_glue::WebMediaPlayerImpl(this);
  WebViewDelegate* d = media_player->GetWebFrame()->GetView()->GetDelegate();

  m_delegate = d->CreateMediaPlayerDelegate();
  // In case we couldn't create a delegate.
  if (m_delegate) {
    m_delegate->Initialize(media_player);
    media_player->Initialize(m_delegate);

    m_delegate->Load(webkit_glue::StringToGURL(url));
  }
}

void MediaPlayerPrivate::cancelLoad() {
  if (m_delegate) {
    m_delegate->CancelLoad();
  }
}

IntSize MediaPlayerPrivate::naturalSize() const {
  if (m_delegate) {
    return IntSize(m_delegate->GetWidth(), m_delegate->GetHeight());
  } else {
    return IntSize(0, 0);
  }
}

bool MediaPlayerPrivate::hasVideo() const {
  if (m_delegate) {
    return m_delegate->IsVideo();
  } else {
    return false;
  }
}

void MediaPlayerPrivate::play() {
  if (m_delegate) {
    m_delegate->Play();
  }
}

void MediaPlayerPrivate::pause() {
  if (m_delegate) {
    m_delegate->Pause();
  }
}

bool MediaPlayerPrivate::paused() const {
  if (m_delegate) {
    return m_delegate->IsPaused();
  } else {
    return true;
  }
}

bool MediaPlayerPrivate::seeking() const {
  if (m_delegate) {
    return m_delegate->IsSeeking();
  } else {
    return false;
  }
}

float MediaPlayerPrivate::duration() const {
  if (m_delegate) {
    return m_delegate->GetDuration();
  } else {
    return 0.0f;
  }
}

float MediaPlayerPrivate::currentTime() const {
  if (m_delegate) {
    return m_delegate->GetCurrentTime();
  } else {
    return 0.0f;
  }
}

void MediaPlayerPrivate::seek(float time) {
  if (m_delegate) {  
    m_delegate->Seek(time);
  }
}

void MediaPlayerPrivate::setEndTime(float time) {
  if (m_delegate) {
    m_delegate->SetEndTime(time);
  }
}

void MediaPlayerPrivate::setRate(float rate) {
  if (m_delegate) {
    m_delegate->SetPlaybackRate(rate);
  }
}

void MediaPlayerPrivate::setVolume(float volume) {
  if (m_delegate) {
    m_delegate->SetVolume(volume);
  }
}

int MediaPlayerPrivate::dataRate() const {
  if (m_delegate) {
    return m_delegate->GetDataRate();
  } else {
    return 0;
  }
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const {
  if (m_delegate) {
    switch (m_delegate->GetNetworkState()) {
      case webkit_glue::WebMediaPlayer::EMPTY:
        return MediaPlayer::Empty;
      case webkit_glue::WebMediaPlayer::LOADED:
        return MediaPlayer::Loaded;
      case webkit_glue::WebMediaPlayer::LOADING:
        return MediaPlayer::Loading;
      case webkit_glue::WebMediaPlayer::LOAD_FAILED:
        return MediaPlayer::LoadFailed;
      case webkit_glue::WebMediaPlayer::LOADED_META_DATA:
        return MediaPlayer::LoadedMetaData;
      case webkit_glue::WebMediaPlayer::LOADED_FIRST_FRAME: 
        return MediaPlayer::LoadedFirstFrame;
      default:
        return MediaPlayer::Empty;
    }
  } else {
    return MediaPlayer::Empty;
  }
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const {
  if (m_delegate) {
    switch (m_delegate->GetReadyState()) {
      case webkit_glue::WebMediaPlayer::CAN_PLAY:
        return MediaPlayer::CanPlay;
      case webkit_glue::WebMediaPlayer::CAN_PLAY_THROUGH:
        return MediaPlayer::CanPlayThrough;
      case webkit_glue::WebMediaPlayer::CAN_SHOW_CURRENT_FRAME:
        return MediaPlayer::CanShowCurrentFrame;
      case webkit_glue::WebMediaPlayer::DATA_UNAVAILABLE:
        return MediaPlayer::DataUnavailable;
      default:
        return MediaPlayer::DataUnavailable;
    }
  } else {
    return MediaPlayer::DataUnavailable;
  }
}

float MediaPlayerPrivate::maxTimeBuffered() const {
  if (m_delegate) {
    return m_delegate->GetMaxTimeBuffered();
  } else {
    return 0.0f;
  }
}

float MediaPlayerPrivate::maxTimeSeekable() const {
  if (m_delegate) {
    return m_delegate->GetMaxTimeSeekable();
  } else {
    return 0.0f;
  }
}

unsigned MediaPlayerPrivate::bytesLoaded() const {
  if (m_delegate) {
    return static_cast<unsigned>(m_delegate->GetBytesLoaded());
  } else {
    return 0;
  }
}

bool MediaPlayerPrivate::totalBytesKnown() const {
  if (m_delegate) {
    return m_delegate->IsTotalBytesKnown();
  } else {
    return false;
  }
}

unsigned MediaPlayerPrivate::totalBytes() const {
  if (m_delegate) {
    return static_cast<unsigned>(m_delegate->GetTotalBytes());
  } else {
    return 0;
  }
}

void MediaPlayerPrivate::setVisible(bool visible) {
  if (m_delegate) {
    m_delegate->SetVisible(visible);
  }
}

void MediaPlayerPrivate::setRect(const IntRect& r) {
  if (m_delegate) {
    m_delegate->SetRect(gfx::Rect(r.x(), r.y(), r.width(), r.height()));
  }
}

void MediaPlayerPrivate::paint(GraphicsContext* p, const IntRect& r) {
  if (m_delegate) {
    gfx::Rect rect(r.x(), r.y(), r.width(), r.height());
    m_delegate->Paint(p->platformContext()->canvas(), rect);
  }
}

// Called from WebMediaPlayer -------------------------------------------------
FrameView* MediaPlayerPrivate::frameView() {
  return m_player->m_frameView;
}

void MediaPlayerPrivate::networkStateChanged() {
  m_player->networkStateChanged();
}

void MediaPlayerPrivate::readyStateChanged() {
  m_player->readyStateChanged();
}

void MediaPlayerPrivate::timeChanged() {
  m_player->timeChanged();
}

void MediaPlayerPrivate::volumeChanged() {
  m_player->volumeChanged();
}

void MediaPlayerPrivate::repaint() {
  m_player->repaint();
}

// public static methods ------------------------------------------------------

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types) {
  // Do nothing for now.
}

bool MediaPlayerPrivate::isAvailable() {
  return webkit_glue::IsMediaPlayerAvailable();
}

}  // namespace WebCore

#endif
