/******************************************************************************
 *  Copyright (c) 2025 The CRTC project authors . All Rights Reserved.
 *
 *  Please visit https://chensongpoixs.github.io for detail
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 ******************************************************************************/
 /*****************************************************************************
				   Author: chensong
				   date:  2025-09-26



 ******************************************************************************/



#ifndef _C_MEDIA_BASE_MEDIA_ENGINE_H_
#define _C_MEDIA_BASE_MEDIA_ENGINE_H_

#include <memory>
#include <string>
#include <vector>
//
//#include "api/audio_codecs/audio_decoder_factory.h"
//#include "api/audio_codecs/audio_encoder_factory.h"
#include "libmedia_transfer_protocol/crypto/crypto_options.h"
#include "libmedia_transfer_protocol/rtp_parameters.h"
#include "libmedia_transfer_protocol/stream_params.h"
#include "api/transport/webrtc_key_value_config.h"
#include "libmedia_codec/video_bitrate_allocator_factory.h"
//#include "call/audio_state.h"
#include "libmedia_transfer_protocol/ccodec.h"
#include "libmedia_transfer_protocol/media_channel.h"
#include "libmedia_codec/video_common.h"
#include "rtc_base/system/file_wrapper.h"

//namespace webrtc {
//class AudioDeviceModule;
//class AudioMixer;
//class AudioProcessing;
//class Call;
//}  // namespace webrtc

namespace libp2p_peerconnection {

webrtc::RTCError CheckRtpParametersValues(
    const libmedia_transfer_protocol::RtpParameters& new_parameters);

webrtc::RTCError CheckRtpParametersInvalidModificationAndValues(
    const libmedia_transfer_protocol::RtpParameters& old_parameters,
    const libmedia_transfer_protocol::RtpParameters& new_parameters);

struct RtpCapabilities {
  RtpCapabilities();
  ~RtpCapabilities();
  std::vector<libmedia_transfer_protocol::RtpExtension> header_extensions;
};

class RtpHeaderExtensionQueryInterface {
 public:
  virtual ~RtpHeaderExtensionQueryInterface() = default;

  // Returns a vector of RtpHeaderExtensionCapability, whose direction is
  // kStopped if the extension is stopped (not used) by default.
  virtual std::vector<libmedia_transfer_protocol::RtpHeaderExtensionCapability>
  GetRtpHeaderExtensions() const = 0;
};

class VoiceEngineInterface : public RtpHeaderExtensionQueryInterface {
 public:
  VoiceEngineInterface() = default;
  virtual ~VoiceEngineInterface() = default;
  RTC_DISALLOW_COPY_AND_ASSIGN(VoiceEngineInterface);

  // Initialization
  // Starts the engine.
  virtual void Init() = 0;

  // TODO(solenberg): Remove once VoE API refactoring is done.
 // virtual rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const = 0;

  // MediaChannel creation
  // Creates a voice media channel. Returns NULL on failure.
  //virtual VoiceMediaChannel* CreateMediaChannel(
  //    webrtc::Call* call,
  //    const MediaConfig& config,
  //    const AudioOptions& options,
  //    const webrtc::CryptoOptions& crypto_options) = 0;

  //virtual const std::vector<AudioCodec>& send_codecs() const = 0;
  //virtual const std::vector<AudioCodec>& recv_codecs() const = 0;

  // Starts AEC dump using existing file, a maximum file size in bytes can be
  // specified. Logging is stopped just before the size limit is exceeded.
  // If max_size_bytes is set to a value <= 0, no limit will be used.
  virtual bool StartAecDump(webrtc::FileWrapper file,
                            int64_t max_size_bytes) = 0;

  // Stops recording AEC dump.
  virtual void StopAecDump() = 0;
};

class VideoEngineInterface : public RtpHeaderExtensionQueryInterface {
 public:
  VideoEngineInterface() = default;
  virtual ~VideoEngineInterface() = default;
  RTC_DISALLOW_COPY_AND_ASSIGN(VideoEngineInterface);

  // Creates a video media channel, paired with the specified voice channel.
  // Returns NULL on failure.
  virtual libmedia_transfer_protocol::VideoMediaChannel* CreateMediaChannel(
     // webrtc::Call* call,
      const libmedia_transfer_protocol::MediaConfig& config,
      const libmedia_transfer_protocol::VideoOptions& options,
      const libmedia_transfer_protocol::CryptoOptions& crypto_options,
      libmedia_codec::VideoBitrateAllocatorFactory*
          video_bitrate_allocator_factory) = 0;

  virtual std::vector<libmedia_transfer_protocol::VideoCodec> send_codecs() const = 0;
  virtual std::vector<libmedia_transfer_protocol::VideoCodec> recv_codecs() const = 0;
};

// MediaEngineInterface is an abstraction of a media engine which can be
// subclassed to support different media componentry backends.
// It supports voice and video operations in the same class to facilitate
// proper synchronization between both media types.
class MediaEngineInterface {
 public:
  virtual ~MediaEngineInterface() {}

  // Initialization. Needs to be called on the worker thread.
  virtual bool Init() = 0;

  virtual VoiceEngineInterface& voice() = 0;
  virtual VideoEngineInterface& video() = 0;
  virtual const VoiceEngineInterface& voice() const = 0;
  virtual const VideoEngineInterface& video() const = 0;
};

// CompositeMediaEngine constructs a MediaEngine from separate
// voice and video engine classes.
// Optionally owns a WebRtcKeyValueConfig trials map.
class CompositeMediaEngine : public MediaEngineInterface {
 public:
  CompositeMediaEngine(std::unique_ptr<webrtc::WebRtcKeyValueConfig> trials,
                       std::unique_ptr<VoiceEngineInterface> audio_engine,
                       std::unique_ptr<VideoEngineInterface> video_engine);
  CompositeMediaEngine(std::unique_ptr<VoiceEngineInterface> audio_engine,
                       std::unique_ptr<VideoEngineInterface> video_engine);
  ~CompositeMediaEngine() override;

  // Always succeeds.
  bool Init() override;

  VoiceEngineInterface& voice() override;
  VideoEngineInterface& video() override;
  const VoiceEngineInterface& voice() const override;
  const VideoEngineInterface& video() const override;

 private:
  const std::unique_ptr<webrtc::WebRtcKeyValueConfig> trials_;
  const std::unique_ptr<VoiceEngineInterface> voice_engine_;
  const std::unique_ptr<VideoEngineInterface> video_engine_;
};

libmedia_transfer_protocol::RtpParameters CreateRtpParametersWithOneEncoding();
libmedia_transfer_protocol::RtpParameters CreateRtpParametersWithEncodings(libmedia_transfer_protocol::StreamParams sp);

// Returns a vector of RTP extensions as visible from RtpSender/Receiver
// GetCapabilities(). The returned vector only shows what will definitely be
// offered by default, i.e. the list of extensions returned from
// GetRtpHeaderExtensions() that are not kStopped.
std::vector<libmedia_transfer_protocol::RtpExtension> GetDefaultEnabledRtpHeaderExtensions(
    const  RtpHeaderExtensionQueryInterface& query_interface);

}  // namespace cricket

#endif  // MEDIA_BASE_MEDIA_ENGINE_H_
