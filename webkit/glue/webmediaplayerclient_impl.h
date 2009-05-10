// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYERCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYERCLIENT_IMPL_H_

#if ENABLE(VIDEO)

#include "webkit/api/public/WebMediaPlayerClient.h"

#include "MediaPlayerPrivate.h"

namespace WebCore {
class MediaPlayerPrivate;
}  // namespace WebCore

namespace WebKit {
class WebMediaPlayer;
}  // namespace WebKit

class WebMediaPlayerClientImpl : public WebKit::WebMediaPlayerClient,
                                 public WebCore::MediaPlayerPrivateInterface {
 public:
  virtual ~WebMediaPlayerClientImpl();

  ////////////////////////////////////////////////////////////////////////////
  // WebMediaPlayerPlayerClient methods
  virtual void networkStateChanged();
  virtual void readyStateChanged();
  virtual void volumeChanged();
  virtual void timeChanged();
  virtual void repaint();
  virtual void durationChanged();
  virtual void rateChanged();
  virtual void sizeChanged();
  virtual void sawUnsupportedTracks();

  ////////////////////////////////////////////////////////////////////////////
  // MediaPlayerPrivateInterface methods
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
  friend class WebCore::MediaPlayerPrivate;

  WebMediaPlayerClientImpl();

  // Static methods used by WebKit for construction.
  static WebCore::MediaPlayerPrivateInterface* create(
      WebCore::MediaPlayer* player);
  static void getSupportedTypes(
      WTF::HashSet<WebCore::String>& supportedTypes);
  static WebCore::MediaPlayer::SupportsType supportsType(
      const WebCore::String& type, const WebCore::String& codecs);

  WebCore::MediaPlayer* m_mediaPlayer;
  WebKit::WebMediaPlayer* m_webMediaPlayer;
};

#endif  // ENABLE(VIDEO)

#endif  // WEBKIT_GLUE_WEBMEDIAPLAYERCLIENT_IMPL_H_
