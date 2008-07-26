// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef CHROME_COMMON_SECURITY_FILTER_PEER_H__
#define CHROME_COMMON_SECURITY_FILTER_PEER_H__

#include "chrome/common/render_messages.h"
#include "webkit/glue/resource_loader_bridge.h"

// The SecurityFilterPeer is a proxy to a
// webkit_glue::ResourceLoaderBridge::Peer instance.  It is used to pre-process
// unsafe resources (such as mixed-content resource).
// Call the factory method CreateSecurityFilterPeer() to obtain an instance of
// SecurityFilterPeer based on the original Peer.
// NOTE: subclasses should insure they delete themselves at the end of the
// OnReceiveComplete call.
class SecurityFilterPeer : public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  virtual ~SecurityFilterPeer();

  static SecurityFilterPeer* CreateSecurityFilterPeer(
      webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      ResourceType::Type resource_type,
      const std::string& mime_type,
      FilterPolicy::Type filter_policy,
      int os_error);

  static SecurityFilterPeer* CreateSecurityFilterPeerForDeniedRequest(
      ResourceType::Type resource_type,
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      int os_error);

  static SecurityFilterPeer* CreateSecurityFilterPeerForFrame(
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      int os_error);

  // ResourceLoaderBridge::Peer methods.
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status);
  virtual std::string GetURLForDebugging();

 protected:
   SecurityFilterPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
                      webkit_glue::ResourceLoaderBridge::Peer* peer);

   webkit_glue::ResourceLoaderBridge* resource_loader_bridge_;
   webkit_glue::ResourceLoaderBridge::Peer* original_peer_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(SecurityFilterPeer);
};

// The BufferedPeer reads all the data of the request into an internal buffer.
// Subclasses should implement DataReady() to process the data as necessary.
class BufferedPeer : public SecurityFilterPeer {
 public:
  BufferedPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
               webkit_glue::ResourceLoaderBridge::Peer* peer,
               const std::string& mime_type);
  virtual ~BufferedPeer();

  // ResourceLoaderBridge::Peer Implementation.
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status);

 protected:
  // Invoked when the entire request has been processed before the data is sent
  // to the original peer, giving an opportunity to subclasses to process the
  // data in data_.  If this method returns true, the data is fed to the
  // original peer, if it returns false, an error is sent instead.
  virtual bool DataReady() = 0;

  webkit_glue::ResourceLoaderBridge::ResponseInfo response_info_;
  std::string data_;

 private:
  std::string mime_type_;
  DISALLOW_EVIL_CONSTRUCTORS(BufferedPeer);
};

// The ReplaceContentPeer cancels the request and serves the provided data as
// content instead.
// TODO (jcampan): we do not as of now cancel the request, as we do not have
// access to the resource_loader_bridge in the SecurityFilterPeer factory
// method.  For now the resource is still being fetched, but ignored, as once
// we have provided the replacement content, the associated pending request
// in ResourceDispatcher is removed and further OnReceived* notifications are
// ignored.
class ReplaceContentPeer : public SecurityFilterPeer {
 public:
  ReplaceContentPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
                     webkit_glue::ResourceLoaderBridge::Peer* peer,
                     const std::string& mime_type,
                     const std::string& data);
  virtual ~ReplaceContentPeer();

  // ResourceLoaderBridge::Peer Implementation.
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info);
  void OnReceivedData(const char* data, int len);
  void OnCompletedRequest(const URLRequestStatus& status);
 private:
   webkit_glue::ResourceLoaderBridge::ResponseInfo response_info_;
   std::string mime_type_;
   std::string data_;
   DISALLOW_EVIL_CONSTRUCTORS(ReplaceContentPeer);
};

// This class filters insecure image by replacing them with a transparent and
// stamped image.
class ImageFilterPeer : public BufferedPeer {
 public:
  ImageFilterPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
                  webkit_glue::ResourceLoaderBridge::Peer* peer);
  virtual ~ImageFilterPeer();

 protected:
   virtual bool DataReady();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ImageFilterPeer);
};

#endif  // CHROME_COMMON_SECURITY_FILTER_PEER_H__
