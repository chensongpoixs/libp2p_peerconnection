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

#include "libp2p_peerconnection/rtp_transport.h"

#include <errno.h>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "libmedia_transfer_protocol/rtp_utils.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/trace_event.h"

namespace libp2p_peerconnection {

void RtpTransport::SetRtcpMuxEnabled(bool enable) {
  rtcp_mux_enabled_ = enable;
  MaybeSignalReadyToSend();
}

const std::string& RtpTransport::transport_name() const {
  return rtp_packet_transport_->transport_name();
}

int RtpTransport::SetRtpOption(rtc::Socket::Option opt, int value) {
  return rtp_packet_transport_->SetOption(opt, value);
}

int RtpTransport::SetRtcpOption(rtc::Socket::Option opt, int value) {
  if (rtcp_packet_transport_) {
    return rtcp_packet_transport_->SetOption(opt, value);
  }
  return -1;
}

void RtpTransport::SetRtpPacketTransport(
	libice::PacketTransportInternal* new_packet_transport) {
  if (new_packet_transport == rtp_packet_transport_) {
    return;
  }
  if (rtp_packet_transport_) {
    rtp_packet_transport_->SignalReadyToSend.disconnect(this);
    rtp_packet_transport_->SignalReadPacket.disconnect(this);
    rtp_packet_transport_->SignalNetworkRouteChanged.disconnect(this);
    rtp_packet_transport_->SignalWritableState.disconnect(this);
    rtp_packet_transport_->SignalSentPacket.disconnect(this);
    // Reset the network route of the old transport.
    SignalNetworkRouteChanged(absl::optional<rtc::NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->SignalReadPacket.connect(this,
                                                   &RtpTransport::OnReadPacket);
    new_packet_transport->SignalNetworkRouteChanged.connect(
        this, &RtpTransport::OnNetworkRouteChanged);
    new_packet_transport->SignalWritableState.connect(
        this, &RtpTransport::OnWritableState);
    new_packet_transport->SignalSentPacket.connect(this,
                                                   &RtpTransport::OnSentPacket);
    // Set the network route for the new transport.
    SignalNetworkRouteChanged(new_packet_transport->network_route());
  }

  rtp_packet_transport_ = new_packet_transport;
  // Assumes the transport is ready to send if it is writable. If we are wrong,
  // ready to send will be updated the next time we try to send.
  SetReadyToSend(false,
                 rtp_packet_transport_ && rtp_packet_transport_->writable());
}

void RtpTransport::SetRtcpPacketTransport(
    libice::PacketTransportInternal* new_packet_transport) {
  if (new_packet_transport == rtcp_packet_transport_) {
    return;
  }
  if (rtcp_packet_transport_) {
    rtcp_packet_transport_->SignalReadyToSend.disconnect(this);
    rtcp_packet_transport_->SignalReadPacket.disconnect(this);
    rtcp_packet_transport_->SignalNetworkRouteChanged.disconnect(this);
    rtcp_packet_transport_->SignalWritableState.disconnect(this);
    rtcp_packet_transport_->SignalSentPacket.disconnect(this);
    // Reset the network route of the old transport.
    SignalNetworkRouteChanged(absl::optional<rtc::NetworkRoute>());
  }
  if (new_packet_transport) {
    new_packet_transport->SignalReadyToSend.connect(
        this, &RtpTransport::OnReadyToSend);
    new_packet_transport->SignalReadPacket.connect(this,
                                                   &RtpTransport::OnReadPacket);
    new_packet_transport->SignalNetworkRouteChanged.connect(
        this, &RtpTransport::OnNetworkRouteChanged);
    new_packet_transport->SignalWritableState.connect(
        this, &RtpTransport::OnWritableState);
    new_packet_transport->SignalSentPacket.connect(this,
                                                   &RtpTransport::OnSentPacket);
    // Set the network route for the new transport.
    SignalNetworkRouteChanged(new_packet_transport->network_route());
  }
  rtcp_packet_transport_ = new_packet_transport;

  // Assumes the transport is ready to send if it is writable. If we are wrong,
  // ready to send will be updated the next time we try to send.
  SetReadyToSend(true,
                 rtcp_packet_transport_ && rtcp_packet_transport_->writable());
}

bool RtpTransport::IsWritable(bool rtcp) const {
	libice::PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                                ? rtcp_packet_transport_
                                                : rtp_packet_transport_;
  return transport && transport->writable();
}

bool RtpTransport::SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                                 const rtc::PacketOptions& options,
                                 int flags) {
  return SendPacket(false, packet, options, flags);
}

bool RtpTransport::SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                                  const rtc::PacketOptions& options,
                                  int flags) {
  return SendPacket(true, packet, options, flags);
}

bool RtpTransport::SendPacket(bool rtcp,
                              rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketOptions& options,
                              int flags) {
	libice::PacketTransportInternal* transport = rtcp && !rtcp_mux_enabled_
                                                ? rtcp_packet_transport_
                                                : rtp_packet_transport_;
  int ret = transport->SendPacket(packet->cdata<char>(), packet->size(),
                                  options, flags);
  if (ret != static_cast<int>(packet->size())) {
    if (transport->GetError() == ENOTCONN) {
      RTC_LOG(LS_WARNING) << "Got ENOTCONN from transport.";
      SetReadyToSend(rtcp, false);
    }
    return false;
  }
  return true;
}

void RtpTransport::UpdateRtpHeaderExtensionMap(
    const libp2p_peerconnection::RtpHeaderExtensions& header_extensions) {
  header_extension_map_ = libmedia_transfer_protocol::RtpHeaderExtensionMap(header_extensions);
}

bool RtpTransport::RegisterRtpDemuxerSink(const webrtc::RtpDemuxerCriteria& criteria,
	webrtc::RtpPacketSinkInterface* sink) {
  rtp_demuxer_.RemoveSink(sink);
  if (!rtp_demuxer_.AddSink(criteria, sink)) {
    RTC_LOG(LS_ERROR) << "Failed to register the sink for RTP demuxer.";
    return false;
  }
  return true;
}

bool RtpTransport::UnregisterRtpDemuxerSink(webrtc::RtpPacketSinkInterface* sink) {
  if (!rtp_demuxer_.RemoveSink(sink)) {
    RTC_LOG(LS_ERROR) << "Failed to unregister the sink for RTP demuxer.";
    return false;
  }
  return true;
}

void RtpTransport::DemuxPacket(rtc::CopyOnWriteBuffer packet,
                               int64_t packet_time_us) {

	// TODO@chensong  2025-09-24   暂时不出来packet demux 逻辑
	RTC_LOG_F(LS_WARNING) << "not demux  TODO @chensong ";
  //webrtc::RtpPacketReceived parsed_packet(
  //    &header_extension_map_, packet_time_us == -1
  //                                ? webrtc::Timestamp::MinusInfinity()
  //                                : webrtc::Timestamp::Micros(packet_time_us));
  //if (!parsed_packet.Parse(std::move(packet))) {
  //  RTC_LOG(LS_ERROR)
  //      << "Failed to parse the incoming RTP packet before demuxing. Drop it.";
  //  return;
  //}
  //
  //if (!rtp_demuxer_.OnRtpPacket(parsed_packet)) {
  //  RTC_LOG(LS_WARNING) << "Failed to demux RTP packet: "
  //                      << webrtc::RtpDemuxer::DescribePacket(parsed_packet);
  //}
} //

bool RtpTransport::IsTransportWritable() {
  auto rtcp_packet_transport =
      rtcp_mux_enabled_ ? nullptr : rtcp_packet_transport_;
  return rtp_packet_transport_ && rtp_packet_transport_->writable() &&
         (!rtcp_packet_transport || rtcp_packet_transport->writable());
}

void RtpTransport::OnReadyToSend(libice::PacketTransportInternal* transport) {
  SetReadyToSend(transport == rtcp_packet_transport_, true);
}

void RtpTransport::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  SignalNetworkRouteChanged(network_route);
}

void RtpTransport::OnWritableState(
   libice:: PacketTransportInternal* packet_transport) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SignalWritableState(IsTransportWritable());
}

void RtpTransport::OnSentPacket(libice::PacketTransportInternal* packet_transport,
                                const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(packet_transport == rtp_packet_transport_ ||
             packet_transport == rtcp_packet_transport_);
  SignalSentPacket(sent_packet);
}

void RtpTransport::OnRtpPacketReceived(rtc::CopyOnWriteBuffer packet,
                                       int64_t packet_time_us) {
  DemuxPacket(packet, packet_time_us);
}

void RtpTransport::OnRtcpPacketReceived(rtc::CopyOnWriteBuffer packet,
                                        int64_t packet_time_us) {
  SignalRtcpPacketReceived(&packet, packet_time_us);
}

void RtpTransport::OnReadPacket(libice::PacketTransportInternal* transport,
                                const char* data,
                                size_t len,
                                const int64_t& packet_time_us,
                                int flags) {
  TRACE_EVENT0("webrtc", "RtpTransport::OnReadPacket");

  // When using RTCP multiplexing we might get RTCP packets on the RTP
  // transport. We check the RTP payload type to determine if it is RTCP.
  auto array_view = rtc::MakeArrayView(data, len);
  libmedia_transfer_protocol::RtpPacketType packet_type = libmedia_transfer_protocol::InferRtpPacketType(array_view);
  // Filter out the packet that is neither RTP nor RTCP.
  if (packet_type == libmedia_transfer_protocol::RtpPacketType::kUnknown) {
    return;
  }

  // Protect ourselves against crazy data.
  if (!libmedia_transfer_protocol::IsValidRtpPacketSize(packet_type, len)) {
    RTC_LOG(LS_ERROR) << "Dropping incoming "
                      << libmedia_transfer_protocol::RtpPacketTypeToString(packet_type)
                      << " packet: wrong size=" << len;
    return;
  }

  rtc::CopyOnWriteBuffer packet(data, len);
  if (packet_type == libmedia_transfer_protocol::RtpPacketType::kRtcp) {
    OnRtcpPacketReceived(std::move(packet), packet_time_us);
  } else {
    OnRtpPacketReceived(std::move(packet), packet_time_us);
  }
}

void RtpTransport::SetReadyToSend(bool rtcp, bool ready) {
  if (rtcp) {
    rtcp_ready_to_send_ = ready;
  } else {
    rtp_ready_to_send_ = ready;
  }

  MaybeSignalReadyToSend();
}

void RtpTransport::MaybeSignalReadyToSend() {
  bool ready_to_send =
      rtp_ready_to_send_ && (rtcp_ready_to_send_ || rtcp_mux_enabled_);
  if (ready_to_send != ready_to_send_) {
    ready_to_send_ = ready_to_send;
    SignalReadyToSend(ready_to_send);
  }
}

}  // namespace webrtc
