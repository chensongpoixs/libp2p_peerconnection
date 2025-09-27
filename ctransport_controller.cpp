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



#include "libp2p_peerconnection/ctransport_controller.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "libice/basic_async_resolver_factory.h"
#include "libice/ice_transport_factory.h"
#include "libice/basic_port_allocator.h"
#include "libice/default_ice_transport_factory.h"
#include "libp2p_peerconnection/connection_context.h"
#include "libice/ice_credentials_iterator.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "libp2p_peerconnection/jsep_transport.h"
namespace libp2p_peerconnection
{
	transport_controller::transport_controller(  rtc::Thread*   t,   rtc::Thread* s
		, rtc::BasicNetworkManager* default_network_manager,
		libice::BasicPacketSocketFactory* default_socket_factory)
		: network_thread_(t)
		, signalie_thread_(s)
		, async_dns_resolver_factory_(std::make_unique<libice::WrappingAsyncDnsResolverFactory>(
            std::make_unique<libice::BasicAsyncResolverFactory>()))
		, ice_transport_factory_(std::make_unique<libice::DefaultIceTransportFactory>())
		, crypto_options_()
		, ices_()
		, dtls_transports_()
		, transports_(
			[this](const std::string& mid, JsepTransport* transport) 
		{
		return OnTransportChanged(mid, transport);
	},
			[this]() {
		RTC_DCHECK_RUN_ON(network_thread_);
		UpdateAggregateStates_n();
	})
		, rtp_rtcp_impl_(nullptr)
	{
		 
		if (network_thread_->IsCurrent())
		{
			port_allocator_ = std::make_shared<libice::BasicPortAllocator>(
				default_network_manager, default_socket_factory,
				nullptr);
			port_allocator_->Initialize();
		}
		else
		{
			network_thread_->PostTask(webrtc::ToQueuedTask(signaling_thread_safety_.flag(), 
				[this, default_network_manager, default_socket_factory]() {
				RTC_DCHECK_RUN_ON(network_thread_);
				port_allocator_ = std::make_shared<libice::BasicPortAllocator>(
					default_network_manager, default_socket_factory,
					nullptr);
				port_allocator_->Initialize();
			}));
		}

#if 1
		//signalie_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
			libmedia_transfer_protocol::RtpRtcpInterface::Configuration   config;
			config.clock = webrtc::Clock::GetRealTimeClock();
			rtp_rtcp_impl_ = std::make_unique<libmedia_transfer_protocol::ModuleRtpRtcpImpl>(config);
		//});
#endif 		
	 
		
		//BasicAsyncResolverFactory
	}
	transport_controller::~transport_controller()
	{
		// 资源释放问题 TODO@chensong  2025-09-25
		//signaling_thread_safety_.~ScopedTaskSafety();
		  rtp_rtcp_impl_.reset();
		 // ice_transport_factory_.reset();
		 // network_thread_->Invoke<void>(RTC_FROM_HERE,[this]() {
		//	  RTC_DCHECK_RUN_ON(network_thread_);
			//  port_allocator_.reset();
			 
		 // });
		 // async_dns_resolver_factory_.reset();
		 
	//	rtp_rtcp_impl_ = nullptr;
	}
	int transport_controller::set_remote_sdp(SessionDescription * desc)
	{
		if (!desc)
		{
			return -1;
		}
		//context_->network_thread()->Invoke<void>( RTC_FROM_HERE,[=](){
			for (size_t i = 0; i < desc->contents_.size(); ++i) {
				std::string mid = desc->contents_[i].name;
				//ContentInfo content = desc->contents_[i];
				RTC_LOG(LS_INFO) << desc->content_groups_[0].ToString();
				if (/*desc->HasGroup(mid) &&*/ mid != desc->contents_[0].name) {
					continue;
				}

			


			
					// 创建ICE transport
				// RTCP, 默认开启a=rtcp:mux
				//ice_agent_->CreateDtlsTransport(mid, 1); // 1: RTP
					
				std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport;
				std::unique_ptr<RtpTransport> unencrypted_rtp_transport;
				std::unique_ptr<libice::DtlsTransportInternal> rtcp_dtls_transport;
				std::unique_ptr<libmedia_transfer_protocol::SctpTransportInternal> sctp_transport;
				std::unique_ptr<SrtpTransport> sdes_transport;
				rtc::scoped_refptr<libice::IceTransportInterface> rtcp_ice;
				rtc::scoped_refptr<libice::IceTransportInterface> ice ;
				std::unique_ptr<libice::DtlsTransportInternal> rtp_dtls_transport;
				libice::DtlsTransportInternal*   rtp_dtls_transport_;
				libice::IceTransportInternal*  ice_p;
				  network_thread_->Invoke< void>(RTC_FROM_HERE, [&] {
					 ice = CreateIceTransport(desc->contents_[i].name, /*rtcp=*/false);
						//  ice 
						RTC_LOG(LS_INFO) << "create ice  name " << mid;
						
						ice_p = ice->internal();
						rtp_dtls_transport =
							CreateDtlsTransport(&desc->contents_[i], ice->internal());
						
						rtp_dtls_transport_ = rtp_dtls_transport.get();
						rtp_dtls_transport_->SetDtlsRole(rtc::SSL_CLIENT);
						dtls_srtp_transport = CreateDtlsSrtpTransport(
							desc->contents_[i].name, rtp_dtls_transport.get(), rtcp_dtls_transport.get());


						std::unique_ptr<JsepTransport> jsep_transport =
							std::make_unique<JsepTransport>(
								desc->contents_[i].name, certificate_, std::move(ice), std::move(rtcp_ice),
								std::move(unencrypted_rtp_transport), std::move(sdes_transport),
								std::move(dtls_srtp_transport), std::move(rtp_dtls_transport),
								std::move(rtcp_dtls_transport), std::move(sctp_transport), [&]() {
							RTC_DCHECK_RUN_ON(network_thread_);
							UpdateAggregateStates_n();
						});

						jsep_transport->rtp_transport()->SignalRtcpPacketReceived.connect(
							this, &transport_controller::OnRtcpPacketReceived_n);

						transports_.RegisterTransport(desc->contents_[i].name, std::move(jsep_transport));
						UpdateAggregateStates_n();
						 
					});


					
				
				ices_.insert(std::make_pair(mid, ice_p));
				dtls_transports_.insert(std::make_pair(mid, rtp_dtls_transport_));
				//dtls_transports_[mid] = rtp_dtls_transport;
				// 设置ICE param
				libice::TransportInfo* td = desc->GetTransportInfoByName(mid);
				if (td) 
				{
					libice::IceParameters  remote_ice_parameter;
					remote_ice_parameter.pwd = td->description.ice_pwd;
					remote_ice_parameter.ufrag = td->description.ice_ufrag;
					//td->description.r
					//content.media_description()
				
					 

					 network_thread_->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [this, remote_ice_parameter, ice_p, rtp_dtls_transport_ = rtp_dtls_transport_, td]() {
						 RTC_DCHECK_RUN_ON(network_thread_);
						 ice_p->SetRemoteIceParameters(remote_ice_parameter);
						 rtp_dtls_transport_->SetRemoteFingerprint(td->description.identity_fingerprint->algorithm,
							td->description.identity_fingerprint->digest.cdata(),
							td->description.identity_fingerprint->digest.size()
						);
					}));
					//ice_agent_->SetRemoteIceParams(mid, 1, libice::IceParameters( td->ice_ufrag, td->ice_pwd));
					//if (td->identity_fingerprint)
					{
						/*	 std::string  fingerprint((char *)td->identity_fingerprint->digest.data()
							 , td->identity_fingerprint->digest.size());*/
						//ice_agent_->SetRemoteFingerprint(td->alg
						//	, td->fingerprint);

						//ice_agent_->SetIceParams
					}
					//auto dtls = _get_dtls_transport(mid);
					//if (dtls) {
					//	dtls->set_remote_fingerprint(td->identity_fingerprint->algorithm,
					//		td->identity_fingerprint->digest.cdata(),
					//		td->identity_fingerprint->digest.size());
					//}
					//ice_agent_->UseDtlsSrtp(mid);
				}

				// 设置ICE candidate
				/*for (auto candidate : content->candidates()) {
					ice_agent_->AddRemoteCandidate(mid, 1, candidate);
				}*/
			}
		//});
		return 0;
	}
	int transport_controller::set_local_sdp(SessionDescription * desc, rtc::scoped_refptr<rtc::RTCCertificate> certificate)
	{

		if (!desc) {
			return -1;
		}
		for (size_t i = 0; i < desc->contents_.size(); ++i) {
			std::string mid = desc->contents_[i].name;
			//ContentInfo content = desc->contents_[i];
			if (mid != desc->contents_[0].name/*desc->HasGroup(mid) && mid != desc->content_groups_[0].semantics_*/) {
				continue;
			}
			libice::TransportInfo* td = desc->GetTransportInfoByName(mid);
			//auto td = desc->GetTransportInfo(mid);
			if (td) {
			/*	ice_agent_->SetIceParams(mid, 1,
					ice::IceParameters(td->ice_ufrag, td->ice_pwd));*/

				libice::IceParameters  local_ice_parameter;
				local_ice_parameter.pwd = td->description.ice_pwd;
				local_ice_parameter.ufrag = td->description.ice_ufrag;
				//td->description.r
				//content.media_description()

				network_thread_->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [mid, local_ice_parameter, this, certificate]() {
					RTC_DCHECK_RUN_ON(network_thread_);
					ices_[mid]->SetIceParameters(local_ice_parameter);
				//	auto ssl_fingerprint = rtc::SSLFingerprint::CreateFromRfc4572(alg, fingerprint);
					dtls_transports_[mid]->SetLocalCertificate(certificate);
				}));
				

				
			}
		}

		 
			if (network_thread_->IsCurrent())
			{
				//	context_->network_thread()->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [this]() {

				for (auto pi : ices_)
				{
					pi.second->MaybeStartGathering();
				}
				

				
					/*webrtc::RTCError error =
						transports_->NegotiateDtlsRole(webrtc::SdpType,
						local_description_->transport_desc.connection_role,
						remote_description_->transport_desc.connection_role,
						&negotiated_dtls_role);*/
				//设置客户端或服务端状态 需要判断双方 连接状态
				//dtls_transports_["audio"]->SetDtlsRole(rtc::SSL_CLIENT);

				//}));
			}
			else
			{
				network_thread_->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [this]() {
					RTC_DCHECK_RUN_ON(network_thread_);
					for (auto pi : ices_)
					{
						pi.second->MaybeStartGathering();
					}
					//dtls_transports_["audio"]->SetDtlsRole(rtc::SSL_CLIENT);
				}));
			}
		 
		
			
		
		return 0;
	}
	int transport_controller::set_remote_candidate(const libice::Candidate & candidate)
	{
		if (network_thread_->IsCurrent())
		{
			//	context_->network_thread()->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [this]() {
			//ices_["audio"]->AddRemoteCandidate(candidate);
			for (auto pi : ices_)
			{
				pi.second->AddRemoteCandidate(candidate);
			}
			//}));
		}
		else
		{
			network_thread_->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [this, candidate]() {
				RTC_DCHECK_RUN_ON(network_thread_);
				for (auto pi : ices_)
				{
					pi.second->AddRemoteCandidate(candidate);
				}
			}));
		}

		
		return 0;
	}
	int transport_controller::send_rtp_packet(const std::string & transport_name, const char * data, size_t len)
	{
	//	auto  * tr = &transports_;
		rtc::CopyOnWriteBuffer buffer(data, len, 2048);// (len, len + 30);
		network_thread_->PostTask(ToQueuedTask(signaling_thread_safety_.flag(), [this, buffer_ = std::move(buffer), transport_name_ = transport_name]()mutable {
			RTC_DCHECK_RUN_ON(network_thread_);
			
			//rtc::PacketOptions  opts;
			//ices_[transport_name]->SendPacket(data, len, opts, 0);
			//return;
			JsepTransport*  jsep_tran = transports_.GetTransportByName(transport_name_);
			if (jsep_tran)
			{
				//rtc::PacketOptions  opts;
				
				//buffer.SetData(data);
				//buffer.SetSize(len + 30);
				//buffer.SetData(std::string(data, len));
				jsep_tran->rtp_transport()->SendRtpPacket(&buffer_, rtc::PacketOptions(), 1);
			}
			/*for (auto pi : ices_)
			{
				rtc::PacketOptions  opts;
				pi.second->SendPacket(data, len, opts, 0);
			}*/
		}));
		return 0;
	}
	int transport_controller::send_rtcp_packet(const std::string & transport_name, const char * data, size_t len)
	{
		return 0;
	}
	void transport_controller::set_certificeate(rtc::scoped_refptr<rtc::RTCCertificate> cert)
	{
		certificate_ = cert;
	}
	bool transport_controller::OnTransportChanged(const std::string & mid, JsepTransport * transport)
	{
		RTC_LOG_F(LS_INFO) << "";
		/*if (config_.transport_observer) {
			if (jsep_transport) {
				return config_.transport_observer->OnTransportChanged(
					mid, jsep_transport->rtp_transport(),
					jsep_transport->RtpDtlsTransport(),
					jsep_transport->data_channel_transport());
			}
			else {
				return config_.transport_observer->OnTransportChanged(mid, nullptr,
					nullptr, nullptr);
			}
		}*/
		return false;
	}
	//void transport_controller::CreateVideoChannel(const libmedia_transfer_protocol::MediaConfig & media_config, 
	//	RtpTransportInternal * rtp_transport, rtc::Thread * signaling_thread, rtc::Thread * worker_thread,
	//	const std::string & content_name, bool srtp_required, const libmedia_transfer_protocol::CryptoOptions & crypto_options, 
	//	rtc::UniqueRandomIdGenerator * ssrc_generator, const libmedia_transfer_protocol::VideoOptions & options, 
	//	libmedia_codec::VideoBitrateAllocatorFactory * video_bitrate_allocator_factory)
	//{
	//	if (!worker_thread->IsCurrent()) {
	//		worker_thread->Invoke<void>(RTC_FROM_HERE, [&] {
	//			  CreateVideoChannel(  media_config, rtp_transport,
	//				signaling_thread, content_name, srtp_required,
	//				crypto_options, ssrc_generator, options,
	//				video_bitrate_allocator_factory);
	//		});
	//	}

	//	RTC_DCHECK_RUN_ON(worker_thread);

	//	libmedia_transfer_protocol::VideoMediaChannel* media_channel = media_engine_->video().CreateMediaChannel(
	//		call, media_config, options, crypto_options,
	//		video_bitrate_allocator_factory);
	//	//if (!media_channel) {
	//	//	return nullptr;
	//	//}
	//}
	rtc::scoped_refptr<libice::IceTransportInterface> transport_controller::CreateIceTransport(const std::string & transport_name, bool rtcp)
	{


		int component = rtcp ? libice::ICE_CANDIDATE_COMPONENT_RTCP
			: libice::ICE_CANDIDATE_COMPONENT_RTP;

		libice::IceTransportInit init;
		init.set_port_allocator(port_allocator_.get());
		init.set_async_dns_resolver_factory(async_dns_resolver_factory_.get());
		//init.set_event_log(config_.event_log);
		return ice_transport_factory_->CreateIceTransport(
			transport_name, component, std::move(init));
		//return rtc::scoped_refptr<libice::IceTransportInterface>();
	}
	std::unique_ptr<libice::DtlsTransportInternal> transport_controller::CreateDtlsTransport(
		 ContentInfo * content_info, libice::IceTransportInternal * ice)
	{
	//	RTC_DCHECK_RUN_ON(context_->signaling_thread());


		std::unique_ptr<libice::DtlsTransportInternal> dtls;

		
		dtls = std::make_unique<libice::DtlsTransport>(ice, crypto_options_,
				nullptr,
			rtc::SSL_PROTOCOL_DTLS_12/*config_.ssl_max_version*/);
		

		RTC_DCHECK(dtls);

		//ice_role_ = DetermineIceRole(, );
		//ice_role_ = libice::ICEROLE_CONTROLLED;
		dtls->ice_transport()->SetIceRole(libice::ICEROLE_CONTROLLED);
		dtls->ice_transport()->SetIceTiebreaker(ice_tiebreaker_);
		dtls->ice_transport()->SetIceConfig(ice_config_);
		if (certificate_) {
			bool set_cert_success = dtls->SetLocalCertificate(certificate_);
			RTC_DCHECK(set_cert_success);
		}

		// Connect to signals offered by the DTLS and ICE transport.
		dtls->SignalWritableState.connect(
			this, &transport_controller::OnTransportWritableState_n);
		dtls->SignalReceivingState.connect(
			this, &transport_controller::OnTransportReceivingState_n);
		dtls->ice_transport()->SignalGatheringState.connect(
			this, &transport_controller::OnTransportGatheringState_n);
		dtls->ice_transport()->SignalCandidateGathered.connect(
			this, &transport_controller::OnTransportCandidateGathered_n);
		dtls->ice_transport()->SignalCandidateError.connect(
			this, &transport_controller::OnTransportCandidateError_n);
		dtls->ice_transport()->SignalCandidatesRemoved.connect(
			this, &transport_controller::OnTransportCandidatesRemoved_n);
		dtls->ice_transport()->SignalRoleConflict.connect(
			this, &transport_controller::OnTransportRoleConflict_n);
		dtls->ice_transport()->SignalStateChanged.connect(
			this, &transport_controller::OnTransportStateChanged_n);
		dtls->ice_transport()->SignalIceTransportStateChanged.connect(
			this, &transport_controller::OnTransportStateChanged_n);
		dtls->ice_transport()->SignalCandidatePairChanged.connect(
			this, &transport_controller::OnTransportCandidatePairChanged_n);

		dtls->SubscribeDtlsHandshakeError(
			[this](rtc::SSLHandshakeError error) { OnDtlsHandshakeError(error); });


		return dtls;
		return std::unique_ptr<libice::DtlsTransportInternal>();
	}
	std::unique_ptr<DtlsSrtpTransport> transport_controller::CreateDtlsSrtpTransport(const std::string & transport_name, libice::DtlsTransportInternal * rtp_dtls_transport, libice::DtlsTransportInternal * rtcp_dtls_transport)
	{

		//RTC_DCHECK_RUN_ON(network_thread_);
		auto dtls_srtp_transport = std::make_unique<DtlsSrtpTransport>(
			rtcp_dtls_transport == nullptr);
		//if (config_.enable_external_auth) {
		//	dtls_srtp_transport->EnableExternalAuth();
		//}

		dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport,
			rtcp_dtls_transport);
		dtls_srtp_transport->SetActiveResetSrtpParams(active_reset_srtp_params_);
		// Capturing this in the callback because JsepTransportController will always
		// outlive the DtlsSrtpTransport.
		dtls_srtp_transport->SetOnDtlsStateChange([this]() {
			RTC_DCHECK_RUN_ON(this->network_thread_);
			this->UpdateAggregateStates_n();
		});
		return dtls_srtp_transport;
		//return std::unique_ptr<DtlsSrtpTransport>();
	}
	void transport_controller::on_ice_stae()
	{
	}
	void transport_controller::on_read_packet(const char * data, size_t len, int64_t ts)
	{
	}

	libice::IceRole transport_controller::DetermineIceRole(const libice::TransportInfo & transport_info, webrtc::SdpType type, bool local)
	{
		libice::IceRole ice_role = ice_role_;
		auto tdesc = transport_info.description;
		if (local) { 
			// ICE  Lite 
			//  两个都设置 LIte 的本地就需要设置 连接 模式  stun rquest 
			/*if (jsep_transport->remote_description() &&
				jsep_transport->remote_description()->transport_desc.ice_mode ==
				libice::ICEMODE_LITE &&
				ice_role_ == libice::ICEROLE_CONTROLLED &&
				tdesc.ice_mode == libice::ICEMODE_FULL)
			{
				ice_role = libice::ICEROLE_CONTROLLING;
			}*/
		}
		else {
			// If our role is cricket::ICEROLE_CONTROLLED and the remote endpoint
			// supports only ice_lite, this local endpoint should take the CONTROLLING
			// role.
			// TODO(deadbeef): This is a session-level attribute, so it really shouldn't
			// be in a TransportDescription in the first place...
			if (ice_role_ == libice::ICEROLE_CONTROLLED &&
				tdesc.ice_mode == libice::ICEMODE_LITE) {
				ice_role = libice::ICEROLE_CONTROLLING;
			}

			// If we use ICE Lite and the remote endpoint uses the full implementation
			// of ICE, the local endpoint must take the controlled role, and the other
			// side must be the controlling role.
			/*if (jsep_transport->local_description() &&
				jsep_transport->local_description()->transport_desc.ice_mode ==
				libice::ICEMODE_LITE &&
				ice_role_ == libice::ICEROLE_CONTROLLING &&
				tdesc.ice_mode == libice::ICEMODE_FULL) {
				ice_role = libice::ICEROLE_CONTROLLED;
			}*/
		}
		ice_role = libice::ICEROLE_CONTROLLED;
		return ice_role;
	}




	void transport_controller::OnTransportWritableState_n(
		libice::PacketTransportInternal* transport) {
		RTC_LOG_F(LS_INFO) << " Transport " << transport->transport_name()
			<< " writability changed to " << transport->writable()
			<< ".";
		UpdateAggregateStates_n();
	}

	void transport_controller::OnTransportReceivingState_n(
		libice::PacketTransportInternal* transport) {
		RTC_LOG_F(LS_INFO) << "";
		UpdateAggregateStates_n();
	}

	void transport_controller::OnTransportGatheringState_n(
		libice::IceTransportInternal* transport) {
		RTC_LOG_F(LS_INFO) << "";
		UpdateAggregateStates_n();
	}

	void transport_controller::OnTransportCandidateGathered_n(
		libice::IceTransportInternal* transport,
		const libice::Candidate& candidate) {
		RTC_LOG_F(LS_INFO) << "candidate = " << candidate.ToString();
		// We should never signal peer-reflexive candidates.
		if (candidate.type() == libice::PRFLX_PORT_TYPE) {
			RTC_NOTREACHED();
			return;
		}

		//signal_ice_candidates_gathered_.Send(
		//	transport->transport_name(), std::vector<cricket::Candidate>{candidate});
	}

	void transport_controller::OnTransportCandidateError_n(
		libice::IceTransportInternal* transport,
		const libice::IceCandidateErrorEvent& event) {
		RTC_LOG_F(LS_INFO) << "";
		//signal_ice_candidate_error_.Send(event);
	}
	void transport_controller::OnTransportCandidatesRemoved_n(
		libice::IceTransportInternal* transport,
		const libice::Candidates& candidates) {
		RTC_LOG_F(LS_INFO) << "";
		//signal_ice_candidates_removed_.Send(candidates);
	}
	void transport_controller::OnTransportCandidatePairChanged_n(
		const libice::CandidatePairChangeEvent& event) {
		RTC_LOG_F(LS_INFO) << "";
		//signal_ice_candidate_pair_changed_.Send(event);
	}


	void transport_controller::OnTransportRoleConflict_n(
		libice::IceTransportInternal* transport) {
		RTC_LOG_F(LS_INFO) << "";
		// Note: since the role conflict is handled entirely on the network thread,
		// we don't need to worry about role conflicts occurring on two ports at
		// once. The first one encountered should immediately reverse the role.
		libice::IceRole reversed_role = (ice_role_ == libice::ICEROLE_CONTROLLING)
			? libice::ICEROLE_CONTROLLED
			: libice::ICEROLE_CONTROLLING;
		RTC_LOG(LS_INFO) << "Got role conflict; switching to "
			<< (reversed_role == libice::ICEROLE_CONTROLLING
				? "controlling"
				: "controlled")
			<< " role.";
		//SetIceRole_n(reversed_role);
	}

	void transport_controller::OnTransportStateChanged_n(
		libice::IceTransportInternal* transport) {
		RTC_LOG_F(LS_INFO) << transport->transport_name() << " Transport "
			<< transport->component()
			<< " state changed. Check if state is complete.";
		UpdateAggregateStates_n();
	}

	void transport_controller::UpdateAggregateStates_n() {
		//TRACE_EVENT0("webrtc", "JsepTransportController::UpdateAggregateStates_n");
		//auto dtls_transports = GetActiveDtlsTransports();
		//libice::IceConnectionState new_connection_state =
		//	libice::kIceConnectionConnecting;
		//PeerConnectionInterface::IceConnectionState new_ice_connection_state =
		//	PeerConnectionInterface::IceConnectionState::kIceConnectionNew;
		//PeerConnectionInterface::PeerConnectionState new_combined_state =
		//	PeerConnectionInterface::PeerConnectionState::kNew;
		//cricket::IceGatheringState new_gathering_state = cricket::kIceGatheringNew;
		//bool any_failed = false;
		//bool all_connected = !dtls_transports.empty();
		//bool all_completed = !dtls_transports.empty();
		//bool any_gathering = false;
		//bool all_done_gathering = !dtls_transports.empty();

		//std::map<IceTransportState, int> ice_state_counts;
		//std::map<DtlsTransportState, int> dtls_state_counts;

		//for (const auto& dtls : dtls_transports) {
		//	any_failed = any_failed || dtls->ice_transport()->GetState() ==
		//		cricket::IceTransportState::STATE_FAILED;
		//	all_connected = all_connected && dtls->writable();
		//	all_completed =
		//		all_completed && dtls->writable() &&
		//		dtls->ice_transport()->GetState() ==
		//		cricket::IceTransportState::STATE_COMPLETED &&
		//		dtls->ice_transport()->GetIceRole() == cricket::ICEROLE_CONTROLLING &&
		//		dtls->ice_transport()->gathering_state() ==
		//		cricket::kIceGatheringComplete;
		//	any_gathering = any_gathering || dtls->ice_transport()->gathering_state() !=
		//		cricket::kIceGatheringNew;
		//	all_done_gathering =
		//		all_done_gathering && dtls->ice_transport()->gathering_state() ==
		//		cricket::kIceGatheringComplete;

		//	dtls_state_counts[dtls->dtls_state()]++;
		//	ice_state_counts[dtls->ice_transport()->GetIceTransportState()]++;
		//}

		//if (any_failed) {
		//	new_connection_state = cricket::kIceConnectionFailed;
		//}
		//else if (all_completed) {
		//	new_connection_state = cricket::kIceConnectionCompleted;
		//}
		//else if (all_connected) {
		//	new_connection_state = cricket::kIceConnectionConnected;
		//}
		//if (ice_connection_state_ != new_connection_state) {
		//	ice_connection_state_ = new_connection_state;

		//	signal_ice_connection_state_.Send(new_connection_state);
		//}

		//// Compute the current RTCIceConnectionState as described in
		//// https://www.w3.org/TR/webrtc/#dom-rtciceconnectionstate.
		//// The PeerConnection is responsible for handling the "closed" state.
		//int total_ice_checking = ice_state_counts[IceTransportState::kChecking];
		//int total_ice_connected = ice_state_counts[IceTransportState::kConnected];
		//int total_ice_completed = ice_state_counts[IceTransportState::kCompleted];
		//int total_ice_failed = ice_state_counts[IceTransportState::kFailed];
		//int total_ice_disconnected =
		//	ice_state_counts[IceTransportState::kDisconnected];
		//int total_ice_closed = ice_state_counts[IceTransportState::kClosed];
		//int total_ice_new = ice_state_counts[IceTransportState::kNew];
		//int total_ice = dtls_transports.size();

		//if (total_ice_failed > 0) {
		//	// Any RTCIceTransports are in the "failed" state.
		//	new_ice_connection_state = PeerConnectionInterface::kIceConnectionFailed;
		//}
		//else if (total_ice_disconnected > 0) {
		//	// None of the previous states apply and any RTCIceTransports are in the
		//	// "disconnected" state.
		//	new_ice_connection_state =
		//		PeerConnectionInterface::kIceConnectionDisconnected;
		//}
		//else if (total_ice_new + total_ice_closed == total_ice) {
		//	// None of the previous states apply and all RTCIceTransports are in the
		//	// "new" or "closed" state, or there are no transports.
		//	new_ice_connection_state = PeerConnectionInterface::kIceConnectionNew;
		//}
		//else if (total_ice_new + total_ice_checking > 0) {
		//	// None of the previous states apply and any RTCIceTransports are in the
		//	// "new" or "checking" state.
		//	new_ice_connection_state = PeerConnectionInterface::kIceConnectionChecking;
		//}
		//else if (total_ice_completed + total_ice_closed == total_ice ||
		//	all_completed) {
		//	// None of the previous states apply and all RTCIceTransports are in the
		//	// "completed" or "closed" state.
		//	//
		//	// TODO(https://bugs.webrtc.org/10356): The all_completed condition is added
		//	// to mimic the behavior of the old ICE connection state, and should be
		//	// removed once we get end-of-candidates signaling in place.
		//	new_ice_connection_state = PeerConnectionInterface::kIceConnectionCompleted;
		//}
		//else if (total_ice_connected + total_ice_completed + total_ice_closed ==
		//	total_ice) {
		//	// None of the previous states apply and all RTCIceTransports are in the
		//	// "connected", "completed" or "closed" state.
		//	new_ice_connection_state = PeerConnectionInterface::kIceConnectionConnected;
		//}
		//else {
		//	RTC_NOTREACHED();
		//}

		//if (standardized_ice_connection_state_ != new_ice_connection_state) {
		//	if (standardized_ice_connection_state_ ==
		//		PeerConnectionInterface::kIceConnectionChecking &&
		//		new_ice_connection_state ==
		//		PeerConnectionInterface::kIceConnectionCompleted) {
		//		// Ensure that we never skip over the "connected" state.
		//		signal_standardized_ice_connection_state_.Send(
		//			PeerConnectionInterface::kIceConnectionConnected);
		//	}
		//	standardized_ice_connection_state_ = new_ice_connection_state;
		//	signal_standardized_ice_connection_state_.Send(new_ice_connection_state);
		//}

		//// Compute the current RTCPeerConnectionState as described in
		//// https://www.w3.org/TR/webrtc/#dom-rtcpeerconnectionstate.
		//// The PeerConnection is responsible for handling the "closed" state.
		//// Note that "connecting" is only a valid state for DTLS transports while
		//// "checking", "completed" and "disconnected" are only valid for ICE
		//// transports.
		//int total_connected =
		//	total_ice_connected + dtls_state_counts[DtlsTransportState::kConnected];
		//int total_dtls_connecting =
		//	dtls_state_counts[DtlsTransportState::kConnecting];
		//int total_failed =
		//	total_ice_failed + dtls_state_counts[DtlsTransportState::kFailed];
		//int total_closed =
		//	total_ice_closed + dtls_state_counts[DtlsTransportState::kClosed];
		//int total_new = total_ice_new + dtls_state_counts[DtlsTransportState::kNew];
		//int total_transports = total_ice * 2;

		//if (total_failed > 0) {
		//	// Any of the RTCIceTransports or RTCDtlsTransports are in a "failed" state.
		//	new_combined_state = PeerConnectionInterface::PeerConnectionState::kFailed;
		//}
		//else if (total_ice_disconnected > 0) {
		//	// None of the previous states apply and any RTCIceTransports or
		//	// RTCDtlsTransports are in the "disconnected" state.
		//	new_combined_state =
		//		PeerConnectionInterface::PeerConnectionState::kDisconnected;
		//}
		//else if (total_new + total_closed == total_transports) {
		//	// None of the previous states apply and all RTCIceTransports and
		//	// RTCDtlsTransports are in the "new" or "closed" state, or there are no
		//	// transports.
		//	new_combined_state = PeerConnectionInterface::PeerConnectionState::kNew;
		//}
		//else if (total_new + total_dtls_connecting + total_ice_checking > 0) {
		//	// None of the previous states apply and all RTCIceTransports or
		//	// RTCDtlsTransports are in the "new", "connecting" or "checking" state.
		//	new_combined_state =
		//		PeerConnectionInterface::PeerConnectionState::kConnecting;
		//}
		//else if (total_connected + total_ice_completed + total_closed ==
		//	total_transports) {
		//	// None of the previous states apply and all RTCIceTransports and
		//	// RTCDtlsTransports are in the "connected", "completed" or "closed" state.
		//	new_combined_state =
		//		PeerConnectionInterface::PeerConnectionState::kConnected;
		//}
		//else {
		//	RTC_NOTREACHED();
		//}

		//if (combined_connection_state_ != new_combined_state) {
		//	combined_connection_state_ = new_combined_state;
		//	signal_connection_state_.Send(new_combined_state);
		//}

		//// Compute the gathering state.
		//if (dtls_transports.empty()) {
		//	new_gathering_state = cricket::kIceGatheringNew;
		//}
		//else if (all_done_gathering) {
		//	new_gathering_state = cricket::kIceGatheringComplete;
		//}
		//else if (any_gathering) {
		//	new_gathering_state = cricket::kIceGatheringGathering;
		//}
		//if (ice_gathering_state_ != new_gathering_state) {
		//	ice_gathering_state_ = new_gathering_state;
		//	signal_ice_gathering_state_.Send(new_gathering_state);
		//}
	}

	void transport_controller::OnDtlsHandshakeError(rtc::SSLHandshakeError error)
	{
		RTC_LOG_F(LS_INFO) << "";
	}

	void transport_controller::OnRtcpPacketReceived_n(rtc::CopyOnWriteBuffer * packet, int64_t packet_time_us)
	{
		RTC_LOG_F(LS_INFO) << "";


		if (rtp_rtcp_impl_)
		{
			//signalie_thread_->PostTask();
			rtc::CopyOnWriteBuffer   bufer(*packet);
			signalie_thread_->PostTask(/*webrtc::ToQueuedTask(signaling_thread_safety_.flag(),*/RTC_FROM_HERE,  
				[this, packet_ = std::move(bufer) ]() {
				RTC_DCHECK_RUN_ON(signalie_thread_);
				rtp_rtcp_impl_->IncomingRtcpPacket(packet_.cdata(), packet_.size());
			});
		}
	}

}
 