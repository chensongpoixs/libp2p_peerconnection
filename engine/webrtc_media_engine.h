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


#ifndef _C_MEDIA_ENGINE_WEBRTC_MEDIA_ENGINE_H_
#define _C_MEDIA_ENGINE_WEBRTC_MEDIA_ENGINE_H_

#include <memory>
#include <string>
#include <vector>
//
//#include "api/audio/audio_frame_processor.h"
//#include "api/audio/audio_mixer.h"
//#include "api/audio_codecs/audio_decoder_factory.h"
//#include "api/audio_codecs/audio_encoder_factory.h"
#include "libmedia_transfer_protocol/rtp_parameters.h"
#include "api/task_queue/task_queue_factory.h"
#include "libmedia_transfer_protocol/bitrate_settings.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/webrtc_key_value_config.h"
#include "libmedia_codec/video_codecs/video_decoder_factory.h"
#include "libmedia_codec/video_codecs/video_encoder_factory.h"
#include "libmedia_transfer_protocol/ccodec.h"
#include "libp2p_peerconnection/engine/media_engine.h"
//#include "modules/audio_device/include/audio_device.h"
//#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/system/rtc_export.h"

namespace libp2p_peerconnection {

struct MediaEngineDependencies {
  MediaEngineDependencies() = default;
  MediaEngineDependencies(const MediaEngineDependencies&) = delete;
  MediaEngineDependencies(MediaEngineDependencies&&) = default;
  MediaEngineDependencies& operator=(const MediaEngineDependencies&) = delete;
  MediaEngineDependencies& operator=(MediaEngineDependencies&&) = default;
  ~MediaEngineDependencies() = default;

  webrtc::TaskQueueFactory* task_queue_factory = nullptr;
  /*rtc::scoped_refptr<webrtc::AudioDeviceModule> adm;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory;
  rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer;
  rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing;
  webrtc::AudioFrameProcessor* audio_frame_processor = nullptr;*/

  std::unique_ptr<libmedia_codec::VideoEncoderFactory> video_encoder_factory;
  std::unique_ptr<libmedia_codec::VideoDecoderFactory> video_decoder_factory;

  const webrtc::WebRtcKeyValueConfig* trials = nullptr;
};

// CreateMediaEngine may be called on any thread, though the engine is
// only expected to be used on one thread, internally called the "worker
// thread". This is the thread Init must be called on.
 std::unique_ptr<MediaEngineInterface> CreateMediaEngine(   MediaEngineDependencies  dependencies);

// Verify that extension IDs are within 1-byte extension range and are not
// overlapping.
bool ValidateRtpExtensions(const std::vector<libmedia_transfer_protocol::RtpExtension>& extensions);

// Discard any extensions not validated by the 'supported' predicate. Duplicate
// extensions are removed if 'filter_redundant_extensions' is set, and also any
// mutually exclusive extensions (see implementation for details) are removed.
std::vector<libmedia_transfer_protocol::RtpExtension> FilterRtpExtensions(
    const std::vector<libmedia_transfer_protocol::RtpExtension>& extensions,
    bool (*supported)(absl::string_view),
    bool filter_redundant_extensions/*,
    const webrtc::WebRtcKeyValueConfig& trials*/);

libmedia_transfer_protocol::BitrateConstraints GetBitrateConfigForCodec(const libmedia_transfer_protocol::Codec& codec);

}  // namespace cricket

#endif  // MEDIA_ENGINE_WEBRTC_MEDIA_ENGINE_H_
