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

#ifndef _C_PC_TRANSPORT_STATS_H_
#define _C_PC_TRANSPORT_STATS_H_

#include <string>
#include <vector>

#include "libice/dtls_transport_interface.h"
#include "libice/dtls_transport_internal.h"
#include "libice/ice_transport_internal.h"
#include "libice/port.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace libp2p_peerconnection {

struct TransportChannelStats {
  TransportChannelStats();
  TransportChannelStats(const TransportChannelStats&);
  ~TransportChannelStats();

  int component = 0;
  int ssl_version_bytes = 0;
  int srtp_crypto_suite = rtc::kSrtpInvalidCryptoSuite;
  int ssl_cipher_suite = rtc::kTlsNullWithNullNull;
  libice::DtlsTransportState dtls_state = libice::DtlsTransportState::kNew;
  libice::IceTransportStats ice_transport_stats;
};

// Information about all the channels of a transport.
// TODO(hta): Consider if a simple vector is as good as a map.
typedef std::vector<TransportChannelStats> TransportChannelStatsList;

// Information about the stats of a transport.
struct TransportStats {
  std::string transport_name;
  TransportChannelStatsList channel_stats;
};

}  // namespace cricket

#endif  // PC_TRANSPORT_STATS_H_
