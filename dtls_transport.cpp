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



#include "libp2p_peerconnection/dtls_transport.h"

#include <utility>

#include "absl/types/optional.h"
#include "libice/dtls_transport_interface.h"
#include "api/sequence_checker.h"
#include "libp2p_peerconnection/ice_transport.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/ssl_certificate.h"

namespace libp2p_peerconnection {

// Implementation of DtlsTransportInterface
DtlsTransport::DtlsTransport(
    std::unique_ptr<libice::DtlsTransportInternal> internal)
    : owner_thread_(rtc::Thread::Current()),
      info_(libice::DtlsTransportState::kNew),
      internal_dtls_transport_(std::move(internal)),
      ice_transport_(rtc::make_ref_counted<IceTransportWithPointer>(
          internal_dtls_transport_->ice_transport())) {
  RTC_DCHECK(internal_dtls_transport_.get());
  internal_dtls_transport_->SubscribeDtlsTransportState(
      [this](libice::DtlsTransportInternal* transport,
		  libice::DtlsTransportState state) {
        OnInternalDtlsState(transport, state);
      });
  UpdateInformation();
}

DtlsTransport::~DtlsTransport() {
  // We depend on the signaling thread to call Clear() before dropping
  // its last reference to this object.
  RTC_DCHECK(owner_thread_->IsCurrent() || !internal_dtls_transport_);
}

libice::DtlsTransportInformation DtlsTransport::Information() {
  webrtc::MutexLock lock(&lock_);
  return info_;
}

void DtlsTransport::RegisterObserver(libice::DtlsTransportObserverInterface* observer) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(observer);
  observer_ = observer;
}

void DtlsTransport::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  observer_ = nullptr;
}

rtc::scoped_refptr<libice::IceTransportInterface> DtlsTransport::ice_transport() {
  return ice_transport_;
}

// Internal functions
void DtlsTransport::Clear() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(internal());
  bool must_send_event =
      (internal()->dtls_state() != libice::DtlsTransportState::kClosed);
  // The destructor of cricket::DtlsTransportInternal calls back
  // into DtlsTransport, so we can't hold the lock while releasing.
  std::unique_ptr<libice::DtlsTransportInternal> transport_to_release;
  {
    webrtc::MutexLock lock(&lock_);
    transport_to_release = std::move(internal_dtls_transport_);
    ice_transport_->Clear();
  }
  UpdateInformation();
  if (observer_ && must_send_event) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::OnInternalDtlsState(
    libice::DtlsTransportInternal* transport,
	libice::DtlsTransportState state) {
  RTC_DCHECK_RUN_ON(owner_thread_);
  RTC_DCHECK(transport == internal());
  RTC_DCHECK(state == internal()->dtls_state());
  UpdateInformation();
  if (observer_) {
    observer_->OnStateChange(Information());
  }
}

void DtlsTransport::UpdateInformation() {
  RTC_DCHECK_RUN_ON(owner_thread_);
  webrtc::MutexLock lock(&lock_);
  if (internal_dtls_transport_) {
    if (internal_dtls_transport_->dtls_state() ==
		libice::DtlsTransportState::kConnected) {
      bool success = true;
      int ssl_cipher_suite;
      int tls_version;
      int srtp_cipher;
      success &= internal_dtls_transport_->GetSslVersionBytes(&tls_version);
      success &= internal_dtls_transport_->GetSslCipherSuite(&ssl_cipher_suite);
      success &= internal_dtls_transport_->GetSrtpCryptoSuite(&srtp_cipher);
      if (success) {
        info_ = libice::DtlsTransportInformation(
            internal_dtls_transport_->dtls_state(), tls_version,
            ssl_cipher_suite, srtp_cipher,
            internal_dtls_transport_->GetRemoteSSLCertChain());
      } else {
        RTC_LOG(LS_ERROR) << "DtlsTransport in connected state has incomplete "
                             "TLS information";
        info_ = libice::DtlsTransportInformation(
            internal_dtls_transport_->dtls_state(), absl::nullopt,
            absl::nullopt, absl::nullopt,
            internal_dtls_transport_->GetRemoteSSLCertChain());
      }
    } else {
      info_ = libice::DtlsTransportInformation(internal_dtls_transport_->dtls_state());
    }
  } else {
    info_ = libice::DtlsTransportInformation(libice::DtlsTransportState::kClosed);
  }
}

}  // namespace webrtc
