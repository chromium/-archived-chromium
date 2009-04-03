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
// TODO(hclam): Have these things done:
//   1. Make AudioManager a singleton and construct/destruct it properly.

#ifndef CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_

#include <map>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_output.h"

class AudioManager;
class MessageLoop;

class AudioRendererHost : public base::RefCountedThreadSafe<AudioRendererHost> {
 private:
  class IPCAudioSource;
  friend class AudioRendererHost::IPCAudioSource;
 public:
  // Called from UI thread from the owner of this object.
  explicit AudioRendererHost(MessageLoop* message_loop);

  // Destruction can happen on either UI thread or IO thread, but at destruction
  // all associated sources are destroyed and streams are closed.
  ~AudioRendererHost();

  // Called from UI thread from the owner of this object to kick start
  // destruction of streams in IO thread.
  void Destroy();

  //---------------------------------------------------------------------------
  // The following public methods are called from ResourceMessageFilter in the
  // IO thread.

  // Creates an audio output stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream about the
  // required properties.
  // ViewMsg_NotifyAudioStreamCreated is sent to renderer if this call is
  // successful, else ViewMsg_NotifyAudioStreamStateChanged with error.
  void CreateStream(base::ProcessHandle process_handle, int32 render_view_id,
                    int32 stream_id, AudioManager::Format format, int channels,
                    int sample_rate, int bits_per_sample, size_t packet_size);

  // Start the audio output strean. ViewMsg_NotifyAudioStreamStateChanged will
  // be sent back to renderer, with
  // ViewMsg_AudioStreamState::AUDIO_STREAM_STARTED upon success,
  // ViewMsg_AudioStreamState::AUDIO_STREAM_ERROR upon error.
  void Start(int32 render_view_id, int32 stream_id);

  // Close the audio output stream. If this call is successful, the
  // AudioOutputStream correspond to (|render_view_id|, |stream_id|) would go
  // unmanaged by this class, subsequent calls to this object with the same
  // (|render_view_id|, |stream_id|) would fail. This method call does not
  // generate a return IPC message back to the renderer.
  void Close(int32 render_view_id, int32 stream_id);

  // Set the volume for the stream specified. No returning IPC message to
  // renderer upon success, ViewMsg_NotifyAudioStreamStateChanged with
  // ViewMsg_AudioStreamState::AUDIO_STREAM_ERROR is sent upon error.
  void SetVolume(int32 render_view_id, int32 stream_id,
      double left_channel, double right_channel);

  // Get the volume of the stream specified. NotifyAudioStreamVolume is sent
  // back to renderer upon success. ViewMsg_NotifyAudioStreamStateChanged with
  // ViewMsg_AudioStreamState::AUDIO_STREAM_ERROR is sent upon error.
  void GetVolume(int32 render_view_id, int32 stream_id);

  // Notify packet has been prepared for stream specified by |stream_id|. The
  // buffer associated with (|render_view_id|, |stream_id|) has been filled and
  // is ready to be consumed.
  void NotifyPacketReady(int32 render_view_id, int32 stream_id,
                         size_t packet_size);

  // Event received when IPC channel is connected with the renderer process.
  void IPCChannelConnected(IPC::Message::Sender* ipc_sender);

  // Event received when IPC channel is closing.
  void IPCChannelClosing();

 private:
  //---------------------------------------------------------------------------
  // Methods called on IO thread.

  // Called on IO thread when this object is created and initialized.
  void OnInitialized();

  // Called on IO thread when this object needs to be destroyed and after
  // Destroy() is called from owner of this class in UI thread.
  void OnDestroyed();

  // Send IPC messages using ipc_sender_.
  void OnSend(IPC::Message* message);

  // A helper method that does the following things:
  // 1. Stop the audio stream contained in the source.
  // 2. Close the audio stream contained in the source.
  // 3. Remove the source from the map.
  // 4. Destruct the source.
  // Destruction of source and associated stream should always be done by this
  // method. *DO NOT* call this method from other than IPCAudioSource and from
  // this class.
  void OnDestroySource(IPCAudioSource* source);

  // A helper method that destroy all IPCAudioSource and associated audio
  // output streams.
  void DestroyAllSources();

  // A helper method to look up a IPCAudioSource with a tuple of render view id
  // and stream id. Returns NULL if not found.
  IPCAudioSource* Lookup(int render_view_id, int stream_id);

  //---------------------------------------------------------------------------
  // Helper methods called from IPCAudioSource or from this class, since
  // methods in IPCAudioSource maybe called from hardware audio threads, these
  // methods make sure the actual tasks happen on IO thread.

  // A helper method to send an IPC message to renderer process on IO thread.
  void Send(IPC::Message* message);

  // A helper method for sending error IPC messages.
  void SendErrorMessage(int32 render_view_id, int32 stream_id, int info);

  // A helper method for calling OnDestroySource on IO thread.
  void DestroySource(IPCAudioSource* source);

  // The container for AudioOutputStream and serves audio packet for it by IPC.
  // This class does nothing more than sending IPC when OnMoreData is called
  // or error is received from the hardware audio thread, it also serves the
  // purpose of containing the audio output stream and associated information.
  // Lifetime of the audio output stream is not controlled by this class.
  class IPCAudioSource : public AudioOutputStream::AudioSourceCallback {
   public:
    IPCAudioSource(AudioRendererHost* host,         // Host of this source.
                   int32 render_view_id,            // Routing ID to RenderView.
                   int32 stream_id,                 // ID of this source.
                   AudioOutputStream* stream,       // Stream associated.
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
    void NotifyPacketReady(size_t packet_size);

    AudioOutputStream* stream() { return stream_; }
    base::SharedMemory* shared_memory() { return &shared_memory_; }
    int32 render_view_id() { return render_view_id_; }
    int32 stream_id() { return stream_id_; }

   private:
    AudioRendererHost* host_;
    int32 render_view_id_;
    int32 stream_id_;
    AudioOutputStream* stream_;
    bool closed_;
    base::SharedMemory shared_memory_;
    base::WaitableEvent packet_read_event_;
    size_t last_packet_size_;
  };

  IPC::Message::Sender* ipc_sender_;

  // A map of id to audio sources.
  typedef std::pair<int32, int32> SourceID;
  typedef std::map<SourceID, IPCAudioSource*> SourceMap;
  SourceMap sources_;

  MessageLoop* io_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
