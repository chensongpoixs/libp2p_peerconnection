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


#ifndef _C_PC_DTLS_SRTP_TRANSPORT_H_
#define _C_PC_DTLS_SRTP_TRANSPORT_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/crypto_params.h"
#include "libice/dtls_transport_interface.h"
#include "api/rtc_error.h"
#include "libice/dtls_transport_internal.h"
#include "libice/packet_transport_internal.h"
#include "libp2p_peerconnection/srtp_transport.h"
#include "rtc_base/buffer.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace libp2p_peerconnection {

// The subclass of SrtpTransport is used for DTLS-SRTP. When the DTLS handshake
// is finished, it extracts the keying materials from DtlsTransport and
// configures the SrtpSessions in the base class.
class DtlsSrtpTransport : public SrtpTransport {
 public:
  explicit DtlsSrtpTransport(bool rtcp_mux_enabled);

  // Set P2P layer RTP/RTCP DtlsTransports. When using RTCP-muxing,
  // `rtcp_dtls_transport` is null.
  void SetDtlsTransports(libice::DtlsTransportInternal* rtp_dtls_transport,
	  libice::DtlsTransportInternal* rtcp_dtls_transport);

  void SetRtcpMuxEnabled(bool enable) override;

  // Set the header extension ids that should be encrypted.
  void UpdateSendEncryptedHeaderExtensionIds(
      const std::vector<int>& send_extension_ids);

  void UpdateRecvEncryptedHeaderExtensionIds(
      const std::vector<int>& recv_extension_ids);

  void SetOnDtlsStateChange(std::function<void(void)> callback);

  webrtc::RTCError SetSrtpSendKey(const cricket::CryptoParams& params) override {
    return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION,
                    "Set SRTP keys for DTLS-SRTP is not supported.");
  }
  webrtc::RTCError SetSrtpReceiveKey(const cricket::CryptoParams& params) override {
    return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION,
                    "Set SRTP keys for DTLS-SRTP is not supported.");
  }

  // If `active_reset_srtp_params_` is set to be true, the SRTP parameters will
  // be reset whenever the DtlsTransports are reset.
  void SetActiveResetSrtpParams(bool active_reset_srtp_params) {
    active_reset_srtp_params_ = active_reset_srtp_params;
  }

 private:
  bool IsDtlsActive();
  bool IsDtlsConnected();
  bool IsDtlsWritable();
  bool DtlsHandshakeCompleted();
  void MaybeSetupDtlsSrtp();
  void SetupRtpDtlsSrtp();
  void SetupRtcpDtlsSrtp();
  bool ExtractParams(libice::DtlsTransportInternal* dtls_transport,
                     int* selected_crypto_suite,
                     rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
                     rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
  void SetDtlsTransport(libice::DtlsTransportInternal* new_dtls_transport,
	  libice::DtlsTransportInternal** old_dtls_transport);
  void SetRtpDtlsTransport(libice::DtlsTransportInternal* rtp_dtls_transport);
  void SetRtcpDtlsTransport(
	  libice::DtlsTransportInternal* rtcp_dtls_transport);

  void OnDtlsState(libice::DtlsTransportInternal* dtls_transport,
	  libice::DtlsTransportState state);

  // Override the SrtpTransport::OnWritableState.
  void OnWritableState(libice::PacketTransportInternal* packet_transport) override;

  // Owned by the TransportController.
  libice::DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  libice::DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;

  // The encrypted header extension IDs.
  absl::optional<std::vector<int>> send_extension_ids_;
  absl::optional<std::vector<int>> recv_extension_ids_;

  bool active_reset_srtp_params_ = false;
  std::function<void(void)> on_dtls_state_change_;
};

}  // namespace webrtc

#endif  // PC_DTLS_SRTP_TRANSPORT_H_
