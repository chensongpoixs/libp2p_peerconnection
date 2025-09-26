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
				   date:  2025-09-25



 ******************************************************************************/




#ifndef _C_CALL_VIDEO_SEND_STREAM_H_
#define _C_CALL_VIDEO_SEND_STREAM_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "libp2p_peerconnection/resource.h"
#include "libmedia_transfer_protocol/transport.h"
#include "libmedia_transfer_protocol/crypto/crypto_options.h"
#include "libmedia_transfer_protocol/frame_transformer_interface.h"
#include "libmedia_transfer_protocol/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "libmedia_codec/video_content_type.h"
#include "libmedia_codec/video_frame.h"
#include "libmedia_codec/video_sink_interface.h"
#include "libmedia_codec/video_source_interface.h"
#include "libmedia_codec/video_stream_encoder_settings.h"
#include "libmedia_codec/video_codecs/video_encoder_config.h"
#include "libp2p_peerconnection/rtp_config.h"
#include "libmedia_codec/frame_counts.h"
#include "libmedia_codec/quality_limitation_reason.h"
#include "libmedia_transfer_protocol/rtp_rtcp/report_block_data.h"
#include "libmedia_transfer_protocol/rtp_rtcp/rtcp_statistics.h"
#include "libmedia_transfer_protocol/rtp_rtcp/rtp_rtcp_defines.h"
namespace libmedia_transfer_protocol
{
	class   FrameEncryptorInterface;
	struct   StreamDataCounters;
	class   Transport;
}
namespace libp2p_peerconnection {

	
		
 

class VideoSendStream {
 public:
  // Multiple StreamStats objects are present if simulcast is used (multiple
  // kMedia streams) or if RTX or FlexFEC is negotiated. Multiple SVC layers, on
  // the other hand, does not cause additional StreamStats.
  struct StreamStats {
    enum class StreamType {
      // A media stream is an RTP stream for audio or video. Retransmissions and
      // FEC is either sent over the same SSRC or negotiated to be sent over
      // separate SSRCs, in which case separate StreamStats objects exist with
      // references to this media stream's SSRC.
      kMedia,
      // RTX streams are streams dedicated to retransmissions. They have a
      // dependency on a single kMedia stream: `referenced_media_ssrc`.
      kRtx,
      // FlexFEC streams are streams dedicated to FlexFEC. They have a
      // dependency on a single kMedia stream: `referenced_media_ssrc`.
      kFlexfec,
    };

    StreamStats();
    ~StreamStats();

    std::string ToString() const;

    StreamType type = StreamType::kMedia;
    // If `type` is kRtx or kFlexfec this value is present. The referenced SSRC
    // is the kMedia stream that this stream is performing retransmissions or
    // FEC for. If `type` is kMedia, this value is null.
    absl::optional<uint32_t> referenced_media_ssrc;
    libmedia_codec::FrameCounts frame_counts;
    int width = 0;
    int height = 0;
    // TODO(holmer): Move bitrate_bps out to the webrtc::Call layer.
    int total_bitrate_bps = 0;
    int retransmit_bitrate_bps = 0;
    int avg_delay_ms = 0;
    int max_delay_ms = 0;
    uint64_t total_packet_send_delay_ms = 0;
	libmedia_transfer_protocol::StreamDataCounters rtp_stats;
	libmedia_transfer_protocol::RtcpPacketTypeCounter rtcp_packet_type_counts;
    // A snapshot of the most recent Report Block with additional data of
    // interest to statistics. Used to implement RTCRemoteInboundRtpStreamStats.
    absl::optional<libmedia_transfer_protocol::ReportBlockData> report_block_data;
    double encode_frame_rate = 0.0;
    int frames_encoded = 0;
    absl::optional<uint64_t> qp_sum;
    uint64_t total_encode_time_ms = 0;
    uint64_t total_encoded_bytes_target = 0;
    uint32_t huge_frames_sent = 0;
  };

  struct Stats {
    Stats();
    ~Stats();
    std::string ToString(int64_t time_ms) const;
    std::string encoder_implementation_name = "unknown";
    int input_frame_rate = 0;
    int encode_frame_rate = 0;
    int avg_encode_time_ms = 0;
    int encode_usage_percent = 0;
    uint32_t frames_encoded = 0;
    // https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-totalencodetime
    uint64_t total_encode_time_ms = 0;
    // https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-totalencodedbytestarget
    uint64_t total_encoded_bytes_target = 0;
    uint32_t frames = 0;
    uint32_t frames_dropped_by_capturer = 0;
    uint32_t frames_dropped_by_encoder_queue = 0;
    uint32_t frames_dropped_by_rate_limiter = 0;
    uint32_t frames_dropped_by_congestion_window = 0;
    uint32_t frames_dropped_by_encoder = 0;
    // Bitrate the encoder is currently configured to use due to bandwidth
    // limitations.
    int target_media_bitrate_bps = 0;
    // Bitrate the encoder is actually producing.
    int media_bitrate_bps = 0;
    bool suspended = false;
    bool bw_limited_resolution = false;
    bool cpu_limited_resolution = false;
    bool bw_limited_framerate = false;
    bool cpu_limited_framerate = false;
    // https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationreason
    libmedia_codec::QualityLimitationReason quality_limitation_reason =
        libmedia_codec::QualityLimitationReason::kNone;
    // https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationdurations
    std::map<libmedia_codec::QualityLimitationReason, int64_t> quality_limitation_durations_ms;
    // https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationresolutionchanges
    uint32_t quality_limitation_resolution_changes = 0;
    // Total number of times resolution as been requested to be changed due to
    // CPU/quality adaptation.
    int number_of_cpu_adapt_changes = 0;
    int number_of_quality_adapt_changes = 0;
    bool has_entered_low_resolution = false;
    std::map<uint32_t, StreamStats> substreams;
	libmedia_codec::VideoContentType content_type =
		libmedia_codec::VideoContentType::UNSPECIFIED;
    uint32_t frames_sent = 0;
    uint32_t huge_frames_sent = 0;
  };

  struct Config {
   public:
    Config() = delete;
    Config(Config&&);
    explicit Config(libmedia_transfer_protocol::Transport* send_transport);

    Config& operator=(Config&&);
    Config& operator=(const Config&) = delete;

    ~Config();

    // Mostly used by tests.  Avoid creating copies if you can.
    Config Copy() const { return Config(*this); }

    std::string ToString() const;

    RtpConfig rtp;

   libmedia_codec:: VideoStreamEncoderSettings encoder_settings;

    // Time interval between RTCP report for video
    int rtcp_report_interval_ms = 1000;

    // Transport for outgoing packets.
	libmedia_transfer_protocol::Transport* send_transport = nullptr;

    // Expected delay needed by the renderer, i.e. the frame will be delivered
    // this many milliseconds, if possible, earlier than expected render time.
    // Only valid if `local_renderer` is set.
    int render_delay_ms = 0;

    // Target delay in milliseconds. A positive value indicates this stream is
    // used for streaming instead of a real-time call.
    int target_delay_ms = 0;

    // True if the stream should be suspended when the available bitrate fall
    // below the minimum configured bitrate. If this variable is false, the
    // stream may send at a rate higher than the estimated available bitrate.
    bool suspend_below_min_bitrate = false;

    // Enables periodic bandwidth probing in application-limited region.
    bool periodic_alr_bandwidth_probing = false;

    // An optional custom frame encryptor that allows the entire frame to be
    // encrypted in whatever way the caller chooses. This is not required by
    // default.
    rtc::scoped_refptr<libmedia_transfer_protocol::FrameEncryptorInterface> frame_encryptor;

    // Per PeerConnection cryptography options.
	libmedia_transfer_protocol:: CryptoOptions crypto_options;

    rtc::scoped_refptr<libmedia_transfer_protocol::FrameTransformerInterface> frame_transformer;

   private:
    // Access to the copy constructor is private to force use of the Copy()
    // method for those exceptional cases where we do use it.
    Config(const Config&);
  };

  // Updates the sending state for all simulcast layers that the video send
  // stream owns. This can mean updating the activity one or for multiple
  // layers. The ordering of active layers is the order in which the
  // rtp modules are stored in the VideoSendStream.
  // Note: This starts stream activity if it is inactive and one of the layers
  // is active. This stops stream activity if it is active and all layers are
  // inactive.
  virtual void UpdateActiveSimulcastLayers(
      const std::vector<bool> active_layers) = 0;

  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;

  // Accessor for determining if the stream is active. This is an inexpensive
  // call that must be made on the same thread as `Start()` and `Stop()` methods
  // are called on and will return `true` iff activity has been started either
  // via `Start()` or `UpdateActiveSimulcastLayers()`. If activity is either
  // stopped or is in the process of being stopped as a result of a call to
  // either `Stop()` or `UpdateActiveSimulcastLayers()` where all layers were
  // deactivated, the return value will be `false`.
  virtual bool started() = 0;

  // If the resource is overusing, the VideoSendStream will try to reduce
  // resolution or frame rate until no resource is overusing.
  // TODO(https://crbug.com/webrtc/11565): When the ResourceAdaptationProcessor
  // is moved to Call this method could be deleted altogether in favor of
  // Call-level APIs only.
  virtual void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) = 0;
  virtual std::vector<rtc::scoped_refptr<Resource>>
  GetAdaptationResources() = 0;

  virtual void SetSource(
	  libmedia_codec::VideoSourceInterface<libmedia_codec::VideoFrame>* source,
      const libmedia_transfer_protocol::DegradationPreference& degradation_preference) = 0;

  // Set which streams to send. Must have at least as many SSRCs as configured
  // in the config. Encoder settings are passed on to the encoder instance along
  // with the VideoStream settings.
  virtual void ReconfigureVideoEncoder(libmedia_codec::VideoEncoderConfig config) = 0;

  virtual Stats GetStats() = 0;

 protected:
  virtual ~VideoSendStream() {}
};

}  // namespace webrtc

#endif  // CALL_VIDEO_SEND_STREAM_H_
