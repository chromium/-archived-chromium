// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioRendererHost serves audio related requests from AudioRenderer which
// lives inside the render process and provide access to audio hardware. It maps
// an internal ID to AudioRendererHost::IPCAudioSource in a map, which is the
// actual object providing audio packets through IPC. It creates the actual
// AudioOutputStream object when requested by the renderer provided with
// render view id and stream id.
//
// AudioRendererHost::IPCAudioSource is a container of AudioOutputStream and
// provide audio packets to the associated AudioOutputStream through IPC. It
// transforms the pull data model to a push model used for IPC. When asked by
// AudioOutputStream for an audio packet, it would send a message to renderer,
// passing a SharedMemoryHandle for filling the buffer.
// NotifyPacketReady(|render_view_id|, |stream_id|) would be called when the
// buffer is filled and ready to be consumed.
//
// This class is owned by BrowserRenderProcessHost, and instantiated on UI
// thread, but all other operations and method calls (except Destroy()) happens
// in IO thread, so we need to be extra careful about the lifetime of this
// object. AudioManager is a singleton and created in IO thread, audio output
// streams are also created in the IO thread, so we need to destroy them also
// in IO thread. After this class is created, a task of OnInitialized() is
// posted on IO thread in which singleton of AudioManager is created and
// AddRef() is called to increase one ref count of this object. Owner of this
// class should call Destroy() before decrementing the ref count to this object,
// which essentially post a task of OnDestroyed() on IO thread. Inside
// OnDestroyed(), audio output streams are destroyed and Release() is called
// which may result in self-destruction.
//
// TODO(hclam): Have these things done before having real implementations:
//   1. Make AudioManager a singleton and construct/destruct it properly.
//   2. Make sure this class has greater or equal lifetime to
//      IPC:Message::Sender, essentially ResourceMessageFilter.
//   3. Listen to destruction event of the browser and do cleanup in case this
//      class is not destructed nicely during browser close.

#ifndef CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_

#include <map>

#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "chrome/common/ipc_message.h"
#include "media/audio/audio_output.h"

class AudioManager;
class MessageLoop;

class AudioRendererHost : public base::RefCountedThreadSafe<AudioRendererHost> {
 public:
  explicit AudioRendererHost(MessageLoop* message_loop);
  ~AudioRendererHost();

  // Creates an audio output stream with the specified format, returns the
  // true if successful. If this call is successful this object would keep an
  // internal entry of the stream about the required properties, renderer
  // process handle and IPC channel for sending buffer request messages.
  bool CreateStream(IPC::Message::Sender* sender, base::ProcessHandle handle,
                    int32 render_view_id, int32 stream_id,
                    AudioManager::Format format, int channels, int sample_rate,
                    int bits_per_sample, size_t packet_size);

  // Start the audio output strean, return false if stream doesn't exist or the
  // cannot start.
  bool Start(int32 render_view_id, int32 stream_id);

  // Stop the audio output stream, return false if stream doesn't exist or
  // cannot stop.
  bool Stop(int32 render_view_id, int32 stream_id);

  // Close the audio output stream, return false if stream doesn't exist or
  // cannot close. If this call is successful, the AudioOutputStream correspond
  // to (|render_view_id|, |stream_id|) would go unmanaged by this class,
  // subsequent calls to this object with the same
  // (|render_view_id|, |stream_id|) would fail.
  bool Close(int32 render_view_id, int32 stream_id);

  // Set the volume for the stream specified, returns true if successful, false
  // if stream doesn't exist or cann't set volume.
  bool SetVolume(int32 render_view_id, int32 stream_id,
      double left_channel, double right_channel);

  // Get the volume of the stream specified, returns true if successful, false
  // is stream doesn't exist or can't get volume.
  bool GetVolume(int32 render_view_id, int32 stream_id,
      double* left_channel, double* right_channel);

  // Notify packet has been prepared for stream specified by |stream_id|. The
  // buffer associated with (|render_view_id|, |stream_id|) has been filled and
  // is ready to be consumed.
  void NotifyPacketReady(int32 render_view_id, int32 stream_id);

  // Called from UI thread from the owner of this object.
  void Destroy();

  // Destroy the stream specified by (|render_view_id|, |stream_id|) and remove
  // it from map.
  // *DO NOT* call this method other than from IPCAudioSource.
  void DestroySource(int32 render_view_id, int32 stream_id);

 private:
  // Methods called on IO thread.
  // Called on IO thread when this object is created and initialized.
  void OnInitialized();
  // Called on IO thread when this object needs to be destroyed and after
  // Destroy() is called from owner of this class in UI thread.
  void OnDestroyed();

  // Destroy all audio output streams.
  void DestroyAllStreams();

  // The container for AudioOutputStream and serves audio packet for it by IPC.
  class IPCAudioSource : public AudioOutputStream::AudioSourceCallback {
   public:
    IPCAudioSource(AudioRendererHost* host,         // Host of this source.
                   int32 render_view_id,            // Routing ID to RenderView.
                   int32 stream_id,                 // ID of this source.
                   AudioOutputStream* stream,       // Stream associated.
                   IPC::Message::Sender* sender,    // IPC sender to user.
                   base::ProcessHandle process,     // Render process handle.
                   size_t packet_size);             // Size of shared memory
                                                    // buffer for writing.
    ~IPCAudioSource();

     // AudioSourceCallback methods.
    virtual size_t OnMoreData(AudioOutputStream* stream,
                              void* dest, size_t max_size);
    virtual void OnClose(AudioOutputStream* stream);
    virtual void OnError(AudioOutputStream* stream, int code);

    // Notify this source that buffer has been filled and is ready to be
    // consumed.
    void NotifyPacketReady();

    int32 render_view_id() { return render_view_id_; }
    int32 stream_id() { return stream_id_; }
    AudioOutputStream* stream() { return stream_; }

   private:
    AudioRendererHost* host_;
    int32 render_view_id_;
    int32 stream_id_;
    AudioOutputStream* stream_;
    IPC::Message::Sender* sender_;
    base::SharedMemory shared_memory_;
    base::SharedMemoryHandle foreign_memory_handle_;
  };

  // Look up a IPCAudioSource with a tuple of render view id and stream id.
  // Return NULL if not found.
  IPCAudioSource* Lookup(int render_view_id, int stream_id);

  // A map of id to audio sources.
  typedef std::pair<int32, int32> SourceID;
  typedef std::map<SourceID, IPCAudioSource*> SourceMap;
  SourceMap sources_;

  // Only used for DCHECKs to make sure all methods calls are from the same
  // thread as this object is created.
  MessageLoop* io_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
