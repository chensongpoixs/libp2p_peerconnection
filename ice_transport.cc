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
				   date:  2025-09-24



 ******************************************************************************/



#include "libp2p_peerconnection/ice_transport.h"

#include "api/sequence_checker.h"

namespace libp2p_peerconnection {

IceTransportWithPointer::~IceTransportWithPointer() {
  // We depend on the networking thread to call Clear() before dropping
  // its last reference to this object; if the destructor is called
  // on the networking thread, it's OK to not have called Clear().
  if (internal_) {
    RTC_DCHECK_RUN_ON(creator_thread_);
  }
}

libice::IceTransportInternal* IceTransportWithPointer::internal() {
  RTC_DCHECK_RUN_ON(creator_thread_);
  return internal_;
}

void IceTransportWithPointer::Clear() {
  RTC_DCHECK_RUN_ON(creator_thread_);
  internal_ = nullptr;
}

}  // namespace webrtc
