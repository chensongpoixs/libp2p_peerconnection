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


#include "libp2p_peerconnection/resource.h"

#include "rtc_base/checks.h"

namespace libp2p_peerconnection {

const char* ResourceUsageStateToString(ResourceUsageState usage_state) {
  switch (usage_state) {
    case ResourceUsageState::kOveruse:
      return "kOveruse";
    case ResourceUsageState::kUnderuse:
      return "kUnderuse";
  }
  RTC_CHECK_NOTREACHED();
}

ResourceListener::~ResourceListener() {}

Resource::Resource() {}

Resource::~Resource() {}

}  // namespace webrtc
