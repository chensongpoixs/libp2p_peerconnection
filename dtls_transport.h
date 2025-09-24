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


				   SctpTransport  --> libp2p_peerconnection/DtlsTransport

				   当前文件专门服务 Sctp协议的SctpTransport
 ******************************************************************************/


#ifndef _C_PC_DTLS_TRANSPORT_H_
#define _C_PC_DTLS_TRANSPORT_H_

#include <memory>

#include "libice/dtls_transport_interface.h"
#include "libice/ice_transport_interface.h"
#include "api/scoped_refptr.h"
#include "libice/dtls_transport.h"
#include "libice/dtls_transport_internal.h"
#include "libp2p_peerconnection/ice_transport.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace libp2p_peerconnection {

//class IceTransportWithPointer;

// This implementation wraps a cricket::DtlsTransport, and takes
// ownership of it.
class DtlsTransport : public libice::DtlsTransportInterface {
 public:
  // This object must be constructed and updated on a consistent thread,
  // the same thread as the one the cricket::DtlsTransportInternal object
  // lives on.
  // The Information() function can be called from a different thread,
  // such as the signalling thread.
  explicit DtlsTransport(
      std::unique_ptr<libice::DtlsTransportInternal> internal);

  rtc::scoped_refptr<libice::IceTransportInterface> ice_transport() override;
  libice::DtlsTransportInformation Information() override;
  void RegisterObserver(libice::DtlsTransportObserverInterface* observer) override;
  void UnregisterObserver() override;
  void Clear();

  libice::DtlsTransportInternal* internal() {
    webrtc::MutexLock lock(&lock_);
    return internal_dtls_transport_.get();
  }

  const libice::DtlsTransportInternal* internal() const {
    webrtc::MutexLock lock(&lock_);
    return internal_dtls_transport_.get();
  }

 protected:
  ~DtlsTransport();

 private:
  void OnInternalDtlsState(libice::DtlsTransportInternal* transport,
	  libice::DtlsTransportState state);
  void UpdateInformation();

  libice::DtlsTransportObserverInterface* observer_ = nullptr;
  rtc::Thread* owner_thread_;
  mutable webrtc::Mutex lock_;
  libice::DtlsTransportInformation info_ RTC_GUARDED_BY(lock_);
  std::unique_ptr<libice::DtlsTransportInternal> internal_dtls_transport_
      RTC_GUARDED_BY(lock_);
  const rtc::scoped_refptr<IceTransportWithPointer> ice_transport_;
};

}  // namespace webrtc
#endif  // PC_DTLS_TRANSPORT_H_
