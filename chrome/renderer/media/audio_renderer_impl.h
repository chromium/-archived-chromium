// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//    AudioRendererHost            AudioRendererImpl
//           ^                             ^
//           |                             |
//           v                 IPC         v
//   ResourceMessageFilter <---------> RenderView
//
// Implementation of interface with audio device is in AudioRendererHost and
// it provides services and entry points in ResourceMessageFilter, allowing
// usage of IPC calls to interact with audio device. RenderView acts as a portal
// for IPC calls and does no more than delegation.
//
// Transportation of audio buffer is done by using shared memory, after
// OnCreateAudioStream is executed, OnCreated would be called along with a
// SharedMemoryHandle upon successful creation of audio output stream in the
// browser process. The same piece of shared memory would be used during the
// lifetime of this unit.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. Render thread
//    The thread within which this class is constructed and destroyed,
//    interfacing with RenderView should only happen here.
// 2. Pipeline thread
//    Initialization of filter and proper stopping of filters happens here.
//    Properties of this filter is also set in this thread.
// 3. Audio decoder thread (If there's one.)
//    Responsible for decoding audio data and gives raw PCM data to this object.
//
// Methods categorized according to the thread(s) they are running on.
//
// Render thread
// +-- CreateFactory()
// |     Helper method for construction this class.
// |-- IsMetidFormatSupported()
// |     Helper method to identify media formats accepted by this class for
// |     construction.
// |-- OnCreateAudioStream()
// |     Calls RenderView::CreateAudioStream().
// |-- OnStartAudioStream()
// |     Calls RenderView::StartAudioStream().
// |-- OnCloseAudioStream()
// |     Calls RenderView::CloseAudioStream().
// |-- OnSetAudioVolume()
// |     Calls RenderView::SetAudioVolume().
// |-- OnNotifyAudioPacketReady
// |     Calls RenderView::NotifyAudioPacketReady().
// |-- OnRequestPacket()
// |     Called from RenderView when an audio packet requested is received
// |     from browser process.
// |-- OnStateChanged()
// |     Called from RenderView upon state change of the audio output stream
// |     in the browser process. Error of the output stream is reported here.
// |-- OnCreated()
// |     Called from RenderView upon successful creation of audio output stream
// |     in the browser process, called along with a SharedMemoryHandle.
// |-- OnVolume()
// |     Called from RenderView about the volume of the audio output stream.
// \-- ReleaseRendererResource()
//       Release resources that live inside render thread.
//
// Pipeline thread
// +-- AudioRendererImpl()
// |     Constructor method.
// |-- ~AudioRendererImpl()
// |     Destructor method.
// |-- SetPlaybackRate()
// |     Given the playback rate information.
// |-- GetMediaFormat()
// |     Obtain the current media format of this unit.
// |-- SetVolume()
// |     Given the volume information.
// |-- OnInitialize()
// |     Called from AudioRendererBase for initialization event.
// \-- OnStop()
//       Called from AudioRendererBase for stop event.
//
// Audio decoder thread (If there's one.)
// \-- OnAssignment()
//       A raw PCM audio packet buffer is recevied here, this method is called
//       from pipeline thread if audio decoder thread does not exist.

#ifndef CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_

#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "media/audio/audio_output.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

class WebMediaPlayerDelegateImpl;

class AudioRendererImpl : public media::AudioRendererBase {
 public:
  // Methods called on render thread ------------------------------------------
  // Methods called during construction.
  static media::FilterFactory* CreateFactory(
      WebMediaPlayerDelegateImpl* delegate) {
    return new media::FilterFactoryImpl1<AudioRendererImpl,
                                         WebMediaPlayerDelegateImpl*>(delegate);
  }
  static bool IsMediaFormatSupported(const media::MediaFormat& format);

  // Methods call from RenderView when audio related IPC messages are received
  // from browser process.
  void OnRequestPacket();
  void OnStateChanged(AudioOutputStream::State state, int info);
  void OnCreated(base::SharedMemoryHandle handle, size_t length);
  void OnVolume(double left, double right);

  // Release resources that lives in renderer thread, i.e. audio output streams.
  void ReleaseRendererResources();

  // Methods called on pipeline thread ----------------------------------------
  // media::MediaFilter implementation.
  virtual void SetPlaybackRate(float rate);

  // media::AudioRenderer implementation.
  virtual void SetVolume(float volume);

  // AssignableBuffer<AudioRendererBase, BufferInterface> implementation.
  virtual void OnAssignment(media::Buffer* buffer_in);

 protected:
  // Methods called on audio renderer thread ----------------------------------
  // These methods are called from AudioRendererBase.
  virtual bool OnInitialize(const media::MediaFormat& media_format);
  virtual void OnStop();

 private:
  friend class media::FilterFactoryImpl1<AudioRendererImpl,
                                         WebMediaPlayerDelegateImpl*>;

  explicit AudioRendererImpl(WebMediaPlayerDelegateImpl* delegate);
  virtual ~AudioRendererImpl();

  // Methods call on render thread --------------------------------------------
  // The following methods are tasks posted on the render thread that needs to
  // be executed on that thread. They interact with WebMediaPlayerDelegateImpl
  // and the containing RenderView, because method calls to RenderView must be
  // executed on render thread.
  void OnCreateAudioStream(AudioManager::Format format, int channels,
                           int sample_rate, int bits_per_sample,
                           size_t packet_size);
  void OnStartAudioStream();
  void OnCloseAudioStream();
  void OnSetAudioVolume(double left, double right);
  void OnNotifyAudioPacketReady();

  WebMediaPlayerDelegateImpl* delegate_;

  // ID of the stream created in the browser process.
  int32 stream_id_;

  // Memory shared by the browser process for audio buffer.
  scoped_ptr<base::SharedMemory> shared_memory_;
  size_t shared_memory_size_;

  // Flag that tells whether we have any unfulfilled packet request.
  bool packet_requested_;

  // Message loop for the render thread, it's the message loop where this class
  // is constructed.
  MessageLoop* render_loop_;

  // Event for releasing resources that live in render thread.
  base::WaitableEvent resource_release_event_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
