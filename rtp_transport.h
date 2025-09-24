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
				   date:  2025-09-21



 ******************************************************************************/

#ifndef _C_PC_RTP_TRANSPORT_H_
#define _C_PC_RTP_TRANSPORT_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "absl/types/optional.h"
#include "call/rtp_demuxer.h"
#include "libmedia_transfer_protocol/rtp_rtcp/rtp_header_extension_map.h"
#include "libice/packet_transport_internal.h"
#include "libp2p_peerconnection/rtp_transport_internal.h"
#include "libp2p_peerconnection/csession_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

 
//namespace libice
//{
//	class PacketTransportInternal;
//}

namespace libp2p_peerconnection {
	//class PacketTransportInternal;
class RtpTransport : public RtpTransportInternal {
 public:
  RtpTransport(const RtpTransport&) = delete;
  RtpTransport& operator=(const RtpTransport&) = delete;

  explicit RtpTransport(bool rtcp_mux_enabled)
      : rtcp_mux_enabled_(rtcp_mux_enabled) {}

  bool rtcp_mux_enabled() const override { return rtcp_mux_enabled_; }
  void SetRtcpMuxEnabled(bool enable) override;

  const std::string& transport_name() const override;

  int SetRtpOption(rtc::Socket::Option opt, int value) override;
  int SetRtcpOption(rtc::Socket::Option opt, int value) override;

  libice::PacketTransportInternal* rtp_packet_transport() const {
    return rtp_packet_transport_;
  }
  void SetRtpPacketTransport(libice::PacketTransportInternal* rtp);

  libice::PacketTransportInternal* rtcp_packet_transport() const {
    return rtcp_packet_transport_;
  }
  void SetRtcpPacketTransport(libice::PacketTransportInternal* rtcp);

  bool IsReadyToSend() const override { return ready_to_send_; }

  bool IsWritable(bool rtcp) const override;

  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;

  bool IsSrtpActive() const override { return false; }

  void UpdateRtpHeaderExtensionMap(
      const libp2p_peerconnection::RtpHeaderExtensions& header_extensions) override;

  bool RegisterRtpDemuxerSink(const webrtc::RtpDemuxerCriteria& criteria,
	  webrtc::RtpPacketSinkInterface* sink) override;

  bool UnregisterRtpDemuxerSink(webrtc::RtpPacketSinkInterface* sink) override;

 protected:
  // These methods will be used in the subclasses.
  void DemuxPacket(rtc::CopyOnWriteBuffer packet, int64_t packet_time_us);

  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options,
                  int flags);

  // Overridden by SrtpTransport.
  virtual void OnNetworkRouteChanged(
      absl::optional<rtc::NetworkRoute> network_route);
  virtual void OnRtpPacketReceived(rtc::CopyOnWriteBuffer packet,
                                   int64_t packet_time_us);
  virtual void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer packet,
                                    int64_t packet_time_us);
  // Overridden by SrtpTransport and DtlsSrtpTransport.
  virtual void OnWritableState(libice::PacketTransportInternal* packet_transport);

 private:
  void OnReadyToSend(libice::PacketTransportInternal* transport);
  void OnSentPacket(libice::PacketTransportInternal* packet_transport,
                    const rtc::SentPacket& sent_packet);
  void OnReadPacket(libice::PacketTransportInternal* transport,
                    const char* data,
                    size_t len,
                    const int64_t& packet_time_us,
                    int flags);

  // Updates "ready to send" for an individual channel and fires
  // SignalReadyToSend.
  void SetReadyToSend(bool rtcp, bool ready);

  void MaybeSignalReadyToSend();

  bool IsTransportWritable();

  bool rtcp_mux_enabled_;

  libice::PacketTransportInternal* rtp_packet_transport_ = nullptr;
  libice::PacketTransportInternal* rtcp_packet_transport_ = nullptr;

  bool ready_to_send_ = false;
  bool rtp_ready_to_send_ = false;
  bool rtcp_ready_to_send_ = false;

  webrtc::RtpDemuxer rtp_demuxer_;

  // Used for identifying the MID for RtpDemuxer.
  libmedia_transfer_protocol::RtpHeaderExtensionMap header_extension_map_;
};

}  // namespace webrtc

#endif  // PC_RTP_TRANSPORT_H_
