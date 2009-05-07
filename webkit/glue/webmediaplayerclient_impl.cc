// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/webmediaplayerclient_impl.h"

#if ENABLE(VIDEO)

#include "third_party/WebKit/WebKit/chromium/public/WebCanvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMediaPlayer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webview_delegate.h"

#include "CString.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "IntSize.h"
#include "KURL.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"

using namespace WebCore;
using namespace WebKit;

WebMediaPlayerClientImpl::WebMediaPlayerClientImpl()
    : m_webMediaPlayer(0) {
}


WebMediaPlayerClientImpl::~WebMediaPlayerClientImpl() {
  delete m_webMediaPlayer;
}

//////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerClientImpl, WebMediaPlayerClient implementations
void WebMediaPlayerClientImpl::networkStateChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->networkStateChanged();
}

void WebMediaPlayerClientImpl::readyStateChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->readyStateChanged();
}

void WebMediaPlayerClientImpl::volumeChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->volumeChanged();
}

void WebMediaPlayerClientImpl::timeChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->timeChanged();
}

void WebMediaPlayerClientImpl::repaint() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->repaint();
}

void WebMediaPlayerClientImpl::durationChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->durationChanged();
}

void WebMediaPlayerClientImpl::rateChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->rateChanged();
}

void WebMediaPlayerClientImpl::sizeChanged() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->sizeChanged();
}

void WebMediaPlayerClientImpl::sawUnsupportedTracks() {
  ASSERT(m_mediaPlayer);
  m_mediaPlayer->mediaPlayerClient()->mediaPlayerSawUnsupportedTracks(
      m_mediaPlayer);
}

//////////////////////////////////////////////////////////////////////////////
// WebMediaPlayerClientImpl, MediaPlayerPrivateInterface implementations
void WebMediaPlayerClientImpl::load(const String& url) {
  delete m_webMediaPlayer;

  WebCore::Frame* frame = static_cast<HTMLMediaElement*>(
      m_mediaPlayer->mediaPlayerClient())->document()->frame();
  WebFrame* webFrame = WebFrameImpl::FromFrame(frame);
  WebViewDelegate* d = webFrame->GetView()->GetDelegate();
  m_webMediaPlayer = d->CreateWebMediaPlayer(this);
  m_webMediaPlayer->load(webkit_glue::KURLToWebURL(KURL(url)));
}

void WebMediaPlayerClientImpl::cancelLoad() {
  if (m_webMediaPlayer)
    m_webMediaPlayer->cancelLoad();
}

void WebMediaPlayerClientImpl::play() {
  if (m_webMediaPlayer)
    m_webMediaPlayer->play();
}

void WebMediaPlayerClientImpl::pause() {
  if (m_webMediaPlayer)
    m_webMediaPlayer->pause();
}

IntSize WebMediaPlayerClientImpl::naturalSize() const {
  if (m_webMediaPlayer) {
    const WebSize& size = m_webMediaPlayer->naturalSize();
    return IntSize(size.width, size.height);
  }
  return IntSize(0, 0);
}

bool WebMediaPlayerClientImpl::hasVideo() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->hasVideo();
  return false;
}

void WebMediaPlayerClientImpl::setVisible(bool visible) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setVisible(visible);
}

float WebMediaPlayerClientImpl::duration() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->duration();
  return 0.0f;
}

float WebMediaPlayerClientImpl::currentTime() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->currentTime();
  return 0.0f;
}

void WebMediaPlayerClientImpl::seek(float time) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->seek(time);
}

bool WebMediaPlayerClientImpl::seeking() const {
  return m_webMediaPlayer->seeking();
}

void WebMediaPlayerClientImpl::setEndTime(float time) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setEndTime(time);
}

void WebMediaPlayerClientImpl::setRate(float rate) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setRate(rate);
}

bool WebMediaPlayerClientImpl::paused() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->paused();
  return false;
}

void WebMediaPlayerClientImpl::setVolume(float volume) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setVolume(volume);
}

MediaPlayer::NetworkState WebMediaPlayerClientImpl::networkState() const {
  COMPILE_ASSERT(
      int(WebMediaPlayer::Empty) == int(MediaPlayer::Empty), Empty);
  COMPILE_ASSERT(
      int(WebMediaPlayer::Idle) == int(MediaPlayer::Idle), Idle);
  COMPILE_ASSERT(
      int(WebMediaPlayer::Loading) == int(MediaPlayer::Loading), Loading);
  COMPILE_ASSERT(
      int(WebMediaPlayer::Loaded) == int(MediaPlayer::Loaded), Loaded);
  COMPILE_ASSERT(
      int(WebMediaPlayer::FormatError) == int(MediaPlayer::FormatError),
      FormatError);
  COMPILE_ASSERT(
      int(WebMediaPlayer::NetworkError) == int(MediaPlayer::NetworkError),
      NetworkError);
  COMPILE_ASSERT(
      int(WebMediaPlayer::DecodeError) == int(MediaPlayer::DecodeError),
      DecodeError);

  if (m_webMediaPlayer)
    return static_cast<MediaPlayer::NetworkState>(
        m_webMediaPlayer->networkState());
  return MediaPlayer::Empty;
}

MediaPlayer::ReadyState WebMediaPlayerClientImpl::readyState() const {
  COMPILE_ASSERT(
      int(WebMediaPlayer::HaveNothing) == int(MediaPlayer::HaveNothing),
      HaveNothing);
  COMPILE_ASSERT(
      int(WebMediaPlayer::HaveMetadata) == int(MediaPlayer::HaveMetadata),
      HaveMetadata);
  COMPILE_ASSERT(
      int(WebMediaPlayer::HaveCurrentData) == int(MediaPlayer::HaveCurrentData),
      HaveCurrentData);
  COMPILE_ASSERT(
      int(WebMediaPlayer::HaveFutureData) == int(MediaPlayer::HaveFutureData),
      HaveFutureData);
  COMPILE_ASSERT(
      int(WebMediaPlayer::HaveEnoughData) == int(MediaPlayer::HaveEnoughData),
      HaveEnoughData);

  if (m_webMediaPlayer)
    return static_cast<MediaPlayer::ReadyState>(m_webMediaPlayer->readyState());
  return MediaPlayer::HaveNothing;
}

float WebMediaPlayerClientImpl::maxTimeSeekable() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->maxTimeSeekable();
  return 0.0f;
}

float WebMediaPlayerClientImpl::maxTimeBuffered() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->maxTimeBuffered();
  return 0.0f;
}

int WebMediaPlayerClientImpl::dataRate() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->dataRate();
  return 0;
}

bool WebMediaPlayerClientImpl::totalBytesKnown() const {
  if (m_webMediaPlayer)
    return m_webMediaPlayer->totalBytesKnown();
  return false;
}

unsigned WebMediaPlayerClientImpl::totalBytes() const {
  if (m_webMediaPlayer)
    return static_cast<unsigned>(m_webMediaPlayer->totalBytes());
  return 0;
}

unsigned WebMediaPlayerClientImpl::bytesLoaded() const {
  if (m_webMediaPlayer)
    return static_cast<unsigned>(m_webMediaPlayer->bytesLoaded());
  return 0;
}

void WebMediaPlayerClientImpl::setSize(const IntSize& size) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setSize(WebSize(size.width(), size.height()));
}

void WebMediaPlayerClientImpl::paint(GraphicsContext* context,
                                     const IntRect& rect) {
// TODO(hclam): enable this for mac.
#if WEBKIT_USING_SKIA
  if (m_webMediaPlayer)
    m_webMediaPlayer->paint(
        context->platformContext()->canvas(),
        WebRect(rect.x(), rect.y(), rect.width(), rect.height()));
#endif
}

void WebMediaPlayerClientImpl::setAutobuffer(bool autoBuffer) {
  if (m_webMediaPlayer)
    m_webMediaPlayer->setAutoBuffer(autoBuffer);
}

// static
MediaPlayerPrivateInterface* WebMediaPlayerClientImpl::create(
    MediaPlayer* player) {
  WebMediaPlayerClientImpl* client = new WebMediaPlayerClientImpl();
  client->m_mediaPlayer = player;
  return client;
}

// static
void WebMediaPlayerClientImpl::getSupportedTypes(
    HashSet<String>& supportedTypes) {
  // TODO(hclam): decide what to do here, we should fill in the HashSet about
  // codecs that we support.
  notImplemented();
}

// static
MediaPlayer::SupportsType WebMediaPlayerClientImpl::supportsType(
    const String& type, const String& codecs) {
  // TODO(hclam): implement this nicely.
  return MediaPlayer::IsSupported;
}

#endif  // ENABLE(VIDEO)
