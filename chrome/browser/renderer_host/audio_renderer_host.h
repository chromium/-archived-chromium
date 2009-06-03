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
// NotifyPacketReady(|route_id|, |stream_id|) would be called when the
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

#ifndef CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_

#include <map>
#include <deque>

#include "base/lock.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "chrome/common/ipc_message.h"
#include "media/audio/audio_output.h"

class AudioManager;
class MessageLoop;
struct ViewHostMsg_Audio_CreateStream;

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

  // Event received when IPC channel is connected with the renderer process.
  void IPCChannelConnected(int process_id, base::ProcessHandle process_handle,
                           IPC::Message::Sender* ipc_sender);

  // Event received when IPC channel is closing.
  void IPCChannelClosing();

  // Returns true if the message is a audio related message and was processed.
  // If it was, message_was_ok will be false iff the message was corrupt.
  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

 private:
  //---------------------------------------------------------------------------
  // Methods called on IO thread.
  // Returns true if the message is an audio related message and should be
  // handled by this class.
  bool IsAudioRendererHostMessage(const IPC::Message& message);

  // Audio related IPC message handlers.
  // Creates an audio output stream with the specified format. If this call is
  // successful this object would keep an internal entry of the stream for the
  // required properties. See IPCAudioSource::CreateIPCAudioSource for more
  // details.
  void OnCreateStream(const IPC::Message& msg, int stream_id,
                      const ViewHostMsg_Audio_CreateStream& params);

  // Starts the audio output stream. Delegates the start method call to the
  // corresponding IPCAudioSource::Start, ViewMsg_NotifyAudioStreamStateChanged
  // with AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnStartStream(const IPC::Message& msg, int stream_id);

  // Closes the audio output stream, delegates the close method call to the
  // corresponding IPCAudioSource::Close, no returning IPC message to renderer
  // upon success and failure.
  void OnCloseStream(const IPC::Message& msg, int stream_id);

  // Set the volume for the stream specified. Delegates the SetVolume method
  // call to IPCAudioSource. No returning IPC message to renderer upon success.
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnSetVolume(const IPC::Message& msg, int stream_id,
                   double left_channel, double right_channel);

  // Gets the volume of the stream specified, delegates to corresponding
  // IPCAudioSource::GetVolume, see the method for more details.
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnGetVolume(const IPC::Message& msg, int stream_id);

  // Notify packet has been prepared for stream, delegates to corresponding
  // IPCAudioSource::NotifyPacketReady, see the method for more details.
  // ViewMsg_NotifyAudioStreamStateChanged with
  // AudioOutputStream::AUDIO_STREAM_ERROR is sent back to renderer if the
  // required IPCAudioSource is not found.
  void OnNotifyPacketReady(const IPC::Message& msg, int stream_id,
                           size_t packet_size);

  // Called on IO thread when this object is created and initialized.
  void OnInitialized();

  // Called on IO thread when this object needs to be destroyed and after
  // Destroy() is called from owner of this class in UI thread.
  void OnDestroyed();

  // Sends IPC messages using ipc_sender_.
  void OnSend(IPC::Message* message);

  // Closes the source, deletes it and removes it from the internal map.
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

  MessageLoop* io_loop() { return io_loop_; }

  // The container for AudioOutputStream and serves audio packet for it by IPC.
  // This class does nothing more than sending IPC when OnMoreData is called
  // or error is received from the hardware audio thread, it also serves the
  // purpose of containing the audio output stream and associated information.
  // Lifetime of the audio output stream is not controlled by this class.
  class IPCAudioSource : public AudioOutputStream::AudioSourceCallback {
   public:
    // Factory method for creating an IPCAudioSource, returns NULL if failed.
    static IPCAudioSource* CreateIPCAudioSource(
        AudioRendererHost* host,             // Host of this source.
        int process_id,                      // Process ID of renderer.
        int route_id,                        // Routing ID to RenderView.
        int stream_id,                       // ID of this source.
        base::ProcessHandle process_handle,  // Process handle of renderer.
        AudioManager::Format format,         // Format of the stream.
        int channels,                        // Number of channels.
        int sample_rate,                     // Sampling frequency/rate.
        char bits_per_sample,                // Number of bits per sample.
        size_t packet_size                   // Number of bytes per packet.
    );
    ~IPCAudioSource();

    // Methods to control playback of the stream.
    // Starts the audio output stream. This method does not call to
    // AudioOutputStream::Start immediately, but instead try get enough initial
    // audio packets from the renderer before actual starting. If pre-rolling
    // has completed and the audio output stream was actually called to start
    // ViewMsg_NotifyAudioStreamStateChanged with
    // AudioOutputStream::AUDIO_STREAM_STARTED is sent back to the renderer.
    void Start();

    // Closes the audio output stream. After calling this method all activities
    // of the audio output stream are stopped.
    void Close();

    // Sets the volume of the audio output stream. There's no IPC messages
    // sent back to the renderer upon success and failure.
    void SetVolume(double left, double right);

    // Gets the volume of the audio output stream.
    // ViewMsg_NotifyAudioStreamVolume is sent back to renderer with volume
    // information if succeeded.
    void GetVolume();

    // Notify this source that buffer has been filled and is ready to be
    // consumed.
    void NotifyPacketReady(size_t packet_size);

    // AudioSourceCallback methods.
    virtual size_t OnMoreData(AudioOutputStream* stream,
                              void* dest, size_t max_size);
    virtual void OnClose(AudioOutputStream* stream);
    virtual void OnError(AudioOutputStream* stream, int code);

    int process_id() { return process_id_; }
    int route_id() { return route_id_; }
    int stream_id() { return stream_id_; }

   private:
    IPCAudioSource(AudioRendererHost* host,     // Host of this source.
                   int process_id,              // Process ID of renderer.
                   int route_id,                // Routing ID to RenderView.
                   int stream_id,               // ID of this source.
                   AudioOutputStream* stream,   // Stream associated.
                   size_t packet_size);         // Size of shared memory
                                                // buffer for writing.
    void StopWaitingForPacket();
    size_t SafeCopyBuffer(void* dest, size_t dest_size,
                          const void* src, size_t src_size);

    AudioRendererHost* host_;
    int process_id_;
    int route_id_;
    int stream_id_;
    AudioOutputStream* stream_;
    size_t packet_size_;

    AudioOutputStream::State state_;
    bool stop_providing_packets_;
    base::SharedMemory shared_memory_;
    base::WaitableEvent packet_read_event_;
    Lock lock_;
    size_t last_packet_size_;
    std::deque<std::pair<uint8*, size_t> > initial_buffers_;
  };

  int process_id_;
  base::ProcessHandle process_handle_;
  IPC::Message::Sender* ipc_sender_;

  // A map of id to audio sources.
  typedef std::pair<int32, int32> SourceID;
  typedef std::map<SourceID, IPCAudioSource*> SourceMap;
  SourceMap sources_;

  MessageLoop* io_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_AUDIO_RENDERER_HOST_H_
