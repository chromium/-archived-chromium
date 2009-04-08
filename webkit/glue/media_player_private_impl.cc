// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "IntRect.h"
#include "MediaPlayerPrivateChromium.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#undef LOG

#include "base/gfx/rect.h"
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

// To avoid exposing a glue type like this in WebCore, MediaPlayerPrivate
// defines a private field m_data as type void*.  This method is used to
// cast that to a WebMediaPlayerDelegate pointer.
static inline webkit_glue::WebMediaPlayerDelegate* AsDelegate(void* data) {
  return static_cast<webkit_glue::WebMediaPlayerDelegate*>(data);
}

// We can't create the delegate here because m_player->frameView is null at
// this moment. Although we can static_cast the MediaPlayerClient to
// HTMLElement and get the frame from there, but creating the delegate from
// load() seems to be a better idea.
MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player),
      m_data(NULL) {
}

MediaPlayerPrivate::~MediaPlayerPrivate() {
  // Delete the delegate, it should delete the associated WebMediaPlayer.
  delete AsDelegate(m_data);
}

void MediaPlayerPrivate::load(const String& url) {
  // Delete the delegate if it already exists. Because we may be in a different
  // view since last load. WebMediaPlayer uses the view internally when
  // using ResourceHandle, WebMediaPlayerDelegate contains the actual media
  // player, in order to hook the actual media player with ResourceHandle in
  // WebMediaPlayer given the new view, we destroy the old
  // WebMediaPlayerDelegate and WebMediaPlayer and create a new set of both.
  delete AsDelegate(m_data);
  m_data = NULL;

  webkit_glue::WebMediaPlayer* media_player =
      new webkit_glue::WebMediaPlayerImpl(this);
  WebViewDelegate* d = media_player->GetWebFrame()->GetView()->GetDelegate();

  webkit_glue::WebMediaPlayerDelegate* new_delegate =
      d->CreateMediaPlayerDelegate();
  // In case we couldn't create a delegate.
  if (new_delegate) {
    m_data = new_delegate;
    new_delegate->Initialize(media_player);
    media_player->Initialize(new_delegate);

    new_delegate->Load(webkit_glue::StringToGURL(url));
  }
}

void MediaPlayerPrivate::cancelLoad() {
  if (m_data)
    AsDelegate(m_data)->CancelLoad();
}

IntSize MediaPlayerPrivate::naturalSize() const {
  if (m_data) {
    return IntSize(AsDelegate(m_data)->GetWidth(),
                   AsDelegate(m_data)->GetHeight());
  } else {
    return IntSize(0, 0);
  }
}

bool MediaPlayerPrivate::hasVideo() const {
  if (m_data) {
    return AsDelegate(m_data)->IsVideo();
  } else {
    return false;
  }
}

void MediaPlayerPrivate::play() {
  if (m_data) {
    AsDelegate(m_data)->Play();
  }
}

void MediaPlayerPrivate::pause() {
  if (m_data) {
    AsDelegate(m_data)->Pause();
  }
}

bool MediaPlayerPrivate::paused() const {
  if (m_data) {
    return AsDelegate(m_data)->IsPaused();
  } else {
    return true;
  }
}

bool MediaPlayerPrivate::seeking() const {
  if (m_data) {
    return AsDelegate(m_data)->IsSeeking();
  } else {
    return false;
  }
}

float MediaPlayerPrivate::duration() const {
  if (m_data) {
    return AsDelegate(m_data)->GetDuration();
  } else {
    return 0.0f;
  }
}

float MediaPlayerPrivate::currentTime() const {
  if (m_data) {
    return AsDelegate(m_data)->GetCurrentTime();
  } else {
    return 0.0f;
  }
}

void MediaPlayerPrivate::seek(float time) {
  if (m_data) {
    AsDelegate(m_data)->Seek(time);
  }
}

void MediaPlayerPrivate::setEndTime(float time) {
  if (m_data) {
    AsDelegate(m_data)->SetEndTime(time);
  }
}

void MediaPlayerPrivate::setRate(float rate) {
  if (m_data) {
    AsDelegate(m_data)->SetPlaybackRate(rate);
  }
}

void MediaPlayerPrivate::setVolume(float volume) {
  if (m_data) {
    AsDelegate(m_data)->SetVolume(volume);
  }
}

int MediaPlayerPrivate::dataRate() const {
  if (m_data) {
    return AsDelegate(m_data)->GetDataRate();
  } else {
    return 0;
  }
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const {
  if (m_data) {
    switch (AsDelegate(m_data)->GetNetworkState()) {
      case webkit_glue::WebMediaPlayer::EMPTY:
        return MediaPlayer::Empty;
      case webkit_glue::WebMediaPlayer::IDLE:
        return MediaPlayer::Idle;
      case webkit_glue::WebMediaPlayer::LOADING:
        return MediaPlayer::Loading;
      case webkit_glue::WebMediaPlayer::LOADED:
        return MediaPlayer::Loaded;
      case webkit_glue::WebMediaPlayer::FORMAT_ERROR:
        return MediaPlayer::FormatError;
      case webkit_glue::WebMediaPlayer::NETWORK_ERROR:
        return MediaPlayer::NetworkError;
      case webkit_glue::WebMediaPlayer::DECODE_ERROR:
        return MediaPlayer::DecodeError;
    }
  }
  return MediaPlayer::Empty;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const {
  if (m_data) {
    switch (AsDelegate(m_data)->GetReadyState()) {
      case webkit_glue::WebMediaPlayer::HAVE_NOTHING:
        return MediaPlayer::HaveNothing;
      case webkit_glue::WebMediaPlayer::HAVE_METADATA:
        return MediaPlayer::HaveMetadata;
      case webkit_glue::WebMediaPlayer::HAVE_CURRENT_DATA:
        return MediaPlayer::HaveCurrentData;
      case webkit_glue::WebMediaPlayer::HAVE_FUTURE_DATA:
        return MediaPlayer::HaveFutureData;
      case webkit_glue::WebMediaPlayer::HAVE_ENOUGH_DATA:
        return MediaPlayer::HaveEnoughData;
    }
  }
  return MediaPlayer::HaveNothing;
}

float MediaPlayerPrivate::maxTimeBuffered() const {
  if (m_data) {
    return AsDelegate(m_data)->GetMaxTimeBuffered();
  } else {
    return 0.0f;
  }
}

float MediaPlayerPrivate::maxTimeSeekable() const {
  if (m_data) {
    return AsDelegate(m_data)->GetMaxTimeSeekable();
  } else {
    return 0.0f;
  }
}

unsigned MediaPlayerPrivate::bytesLoaded() const {
  if (m_data) {
    return static_cast<unsigned>(AsDelegate(m_data)->GetBytesLoaded());
  } else {
    return 0;
  }
}

bool MediaPlayerPrivate::totalBytesKnown() const {
  if (m_data) {
    return AsDelegate(m_data)->IsTotalBytesKnown();
  } else {
    return false;
  }
}

unsigned MediaPlayerPrivate::totalBytes() const {
  if (m_data) {
    return static_cast<unsigned>(AsDelegate(m_data)->GetTotalBytes());
  } else {
    return 0;
  }
}

void MediaPlayerPrivate::setVisible(bool visible) {
  if (m_data) {
    AsDelegate(m_data)->SetVisible(visible);
  }
}

void MediaPlayerPrivate::setSize(const IntSize& size) {
  if (m_data) {
    AsDelegate(m_data)->SetSize(gfx::Size(size.width(), size.height()));
  }
}

void MediaPlayerPrivate::paint(GraphicsContext* p, const IntRect& r) {
  if (m_data) {
    gfx::Rect rect(r.x(), r.y(), r.width(), r.height());
    AsDelegate(m_data)->Paint(p->platformContext()->canvas(), rect);
  }
}

// Called from WebMediaPlayer -------------------------------------------------
FrameView* MediaPlayerPrivate::frameView() {
  return m_player->frameView();
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

void MediaPlayerPrivate::sizeChanged() {
  m_player->sizeChanged();
}

void MediaPlayerPrivate::rateChanged() {
  m_player->rateChanged();
}

void MediaPlayerPrivate::durationChanged() {
  m_player->durationChanged();
}

// public static methods ------------------------------------------------------

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player) {
  return new MediaPlayerPrivate(player);
}

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar) {
  if (webkit_glue::IsMediaPlayerAvailable())
    registrar(create, getSupportedTypes, supportsType);
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(
    const String &type, const String &codecs) {
  // TODO(hclam): decide what to do here, now we just say we support everything.
  return MediaPlayer::IsSupported;
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types) {
  // TODO(hclam): decide what to do here, we should fill in the HashSet about
  // codecs that we support.
  notImplemented();
}

}  // namespace WebCore

#endif
