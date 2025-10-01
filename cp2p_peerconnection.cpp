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
#include "libp2p_peerconnection/cp2p_peerconnection.h"
#include "api/jsep.h"
#include "pc/webrtc_sdp.h"
#include "libice/candidate.h"
#include "libice/default_ice_transport_factory.h"
#include "libice/ice_credentials_iterator.h"
#include "libmedia_transfer_protocol/rtp_rtcp/rtp_packet_to_send.h"
#include "libmedia_transfer_protocol/rtp_rtcp/rtp_format.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "libmedia_transfer_protocol/rtp_rtcp/rtp_rtcp_defines.h"
#include "libice/network_types.h"
//#include "libmedia_codec/builtin_video_bitrate_allocator_factory.h"
//#include "libp2p_peerconnection/engine/webrtc_media_engine.h"
namespace libp2p_peerconnection
{
	const size_t RTC_PACKET_CACHE_SIZE = 2048;
	namespace {
		// a=attr_name:attr_value
		static std::string GetAttribute(const std::string& line) {
			std::vector<std::string> fields;
			size_t size = rtc::tokenize(line, ':', &fields);
			if (size != 2) {
				RTC_LOG(LS_WARNING) << "get attribute failed: " << line;
				return "";
			}

			return fields[1];
		}
		static bool ParseCandidates(MediaContentDescription* media_content,
			const std::string& line, std::vector< libice::Candidate> & all_c)
		{
			if (line.find("a=candidate:") == std::string::npos) {
				return true;
			}

			std::string attr_value = GetAttribute(line);
			if (attr_value.empty()) {
				return false;
			}

			std::vector<std::string> fields;
			size_t size = rtc::tokenize(attr_value, ' ', &fields);
			if (size < 8) {
				return false;
			}

			libice::Candidate  c;
			c.set_foundation(  fields[0]);
			c.set_component(  std::atoi(fields[1].c_str()));
			c.set_protocol(  fields[2]);
			c.set_priority(   std::atoi(fields[3].c_str()));
			uint16_t  port = std::atoi(fields[5].c_str());
			c.set_address(  rtc::SocketAddress(fields[4], port));
			c.set_type(  fields[7]);
			all_c.push_back(c);
			//media_content->AddCandidate(c);
			return true;
		}



		static bool ParseTransportInfo(libice::TransportDescription* td,
			const std::string& line)
		{
			if (line.find("a=ice-ufrag") != std::string::npos) {
				td->ice_ufrag = GetAttribute(line);
				if (td->ice_ufrag.empty()) {
					return false;
				}
			}
			else if (line.find("a=ice-pwd") != std::string::npos) {
				td->ice_pwd = GetAttribute(line);
				if (td->ice_pwd.empty()) {
					return false;
				}
			}
			else if (line.find("a=fingerprint") != std::string::npos) {
				std::vector<std::string> items;
				rtc::tokenize(line, ' ', &items);
				if (items.size() != 2) {
					RTC_LOG(LS_WARNING) << "parse a=fingerprint error: " << line;
					return -1;
				}

				// a=fingerprint: 14
				std::string alg = items[0].substr(14);
				absl::c_transform(alg, alg.begin(), ::tolower);
				td->identity_fingerprint = rtc::SSLFingerprint::CreateUniqueFromRfc4572(
					alg, items[1]);
				if(!(td->identity_fingerprint.get()))
				{
					RTC_LOG(LS_WARNING) << "create fingerprint error: " << line;
					return -1;
				}
				//td->fingerprint = items[1];
				//std::string   content   = items[1];
				//td->alg = alg;
				/*td->identity_fingerprint = rtc::SSLFingerprint::CreateUniqueFromRfc4572(
					alg, content);
				if (!(td->identity_fingerprint.get()))
				{
					RTC_LOG(LS_WARNING) << "create fingerprint error: " << line;
					return -1;
				}*/
			}


			return true;
		}




		static libmedia_transfer_protocol::RtpTransceiverDirection GetDirection(bool send, bool recv) {
			if (send && recv) {
				return libmedia_transfer_protocol::RtpTransceiverDirection::kSendRecv;
			}
			else if (send && !recv) {
				return libmedia_transfer_protocol::RtpTransceiverDirection::kSendOnly;
			}
			else if (!send && recv) {
				return libmedia_transfer_protocol::RtpTransceiverDirection::kRecvOnly;
			}
			else {
				return libmedia_transfer_protocol::RtpTransceiverDirection::kInactive;
			}
		}
	}

	p2p_peer_connection::p2p_peer_connection()
		: context_(ConnectionContext::Create())
		, transport_controller_(nullptr)
		//, signaling_thread_safety_()
		, video_cache_(RTC_PACKET_CACHE_SIZE)
		, transport_send_(nullptr)
		//, media_engine_(nullptr)
		//, video_bitrate_allocator_factory_ (libmedia_codec::CreateBuiltinVideoBitrateAllocatorFactory())
		, rtp_rtcp_impl_(nullptr)
		, task_queue_factory_(webrtc::CreateDefaultTaskQueueFactory())
	{

		if (context_->network_thread()->IsCurrent())
		{
			
			transport_controller_ = std::make_unique<transport_controller>(context_->network_thread()
				, context_->signaling_thread(), context_->default_network_manager(), 
				context_->default_socket_factory());
			transport_controller_->SignalIceTransportStateChanged.connect(this, & p2p_peer_connection::IceTransportStateChanged_n);
			transport_controller_->SignalRtcpPacketReceived.connect(
				this, & p2p_peer_connection::OnRtcpPacketReceived_n);
		}
		else
		{
			context_->network_thread()->PostTask(RTC_FROM_HERE, [this]() {
				RTC_DCHECK_RUN_ON(context_->network_thread());
				
				transport_controller_ = std::make_unique<transport_controller>(context_->network_thread()
					, context_->signaling_thread(), context_->default_network_manager(),
					context_->default_socket_factory());
				
				transport_controller_->SignalIceTransportStateChanged.connect(this, & p2p_peer_connection::IceTransportStateChanged_n);
				transport_controller_->SignalRtcpPacketReceived.connect(
					this, & p2p_peer_connection::OnRtcpPacketReceived_n);
			});
			/*context_->signaling_thread()->Invoke<void>(RTC_FROM_HERE, [&]() {
				RTC_DCHECK_RUN_ON(context_->signaling_thread());
				transport_controller_ = std::make_unique<transport_controller>(context_);
			});*/
		/*	context_->signaling_thread()->Invoke<void>(RTC_FROM_HERE, [&]() {
				RTC_DCHECK_RUN_ON(context_->signaling_thread());
				transport_controller_ = std::make_unique<transport_controller>(context_);
			});*/
			
		}


		
		//media_engine_ = libp2p_peerconnection::CreateMediaEngine::Create(media_dep);
		//context_->worker_thread()->PostTask(RTC_FROM_HERE, [this]() {
		//	RTC_DCHECK_RUN_ON(context_->network_thread());
		//	libp2p_peerconnection::MediaEngineDependencies media_dep;
		//	//using namespace libp2p_peerconnection;
		//	media_engine_ = libp2p_peerconnection::CreateMediaEngine(media_dep);
		//}
		//);


  // 注册 transport-cc  rtp header
		rtp_header_extension_map_.Register<libmedia_transfer_protocol::TransportSequenceNumber>(libmedia_transfer_protocol::kRtpExtensionTransportSequenceNumber);
		
	}
	p2p_peer_connection::~p2p_peer_connection()
	{
		context_->network_thread()->Invoke<void>(RTC_FROM_HERE, [this]() {
			RTC_DCHECK_RUN_ON(context_->network_thread());
			  transport_controller_.reset(nullptr);
		});
		context_->Release();
		//context_->network_thread()->Stop();
		//context_->signaling_thread()->Stop();
		//context_->worker_thread()->Stop();
	}
	int p2p_peer_connection::set_remote_sdp(const std::string & sdp)
	{

		{
			const uint64_t k_year_in_ms = 365 * 24 * 3600 * 1000L;
			ice_param_ = libice::IceCredentialsIterator::CreateRandomIceCredentials();
			//std::string cname = rtc::CreateRandomString(16);
			rtc::KeyParams key_params;
			RTC_LOG(LS_INFO) << "dtls enabled, key type: " << key_params.type();
			certificate_ = rtc::RTCCertificateGenerator::GenerateCertificate(key_params,
				k_year_in_ms);
			//SSLFingerprint
			if (certificate_)
			{
				rtc::RTCCertificatePEM pem = certificate_->ToPEM();
				RTC_LOG(LS_INFO) << "rtc certificate: \n" << pem.certificate();
				//certificate_ = certificate;
				transport_controller_->set_certificeate(certificate_);
			}
			else
			{
				RTC_LOG(LS_WARNING) << " open  certificate failed !!!\n";
			}
		}
		 
		
		std::vector<std::string> fields;
		// SDP用\n, \r\n来换行的
		rtc::tokenize(sdp, '\n', &fields);
		if (fields.size() <= 0) {
			RTC_LOG(LS_WARNING) << "invalid sdp: " << sdp;
			return -1;
		}

		// 判断是否是\r\n换行
		bool is_rn = false;
		if (sdp.find("\r\n") != std::string::npos) {
			is_rn = true;
		}

		remote_desc_ = std::make_unique < SessionDescription > ();
		//remote_desc_->set_msid_supported(false);
		//remote_desc_->set_extmap_allow_mixed(false);
		std::string mid;
		auto audio_content = std::make_unique<AudioContentDescription>();
		auto video_content = std::make_unique<VideoContentDescription>();
		//auto audio_td = std::make_unique<TransportDescription>();
		libice::TransportInfo  audio_td;
		//auto video_td = std::make_unique<TransportDescription>();
		libice::TransportInfo  video_td;
		std::vector<libice::Candidate>  all_candidate;
	//	libice::Candidate audio_c;
		//libice::Candidate video_c;
		ContentInfo  audio_content_info;
		
		ContentInfo video_content_info;
		//remote_desc_->contents_.push_back(content_info);
		for (auto field : fields) {
			// 如果以\r\n换行，去掉尾部的\r
			if (is_rn) {
				field = field.substr(0, field.length() - 1);
			}

			if (field.find("a=group:BUNDLE") != std::string::npos) {
				std::vector<std::string> items;
				rtc::tokenize(field, ' ', &items);
				if (items.size() > 1) {
					ContentGroup  offer_bundle;// = new ContentGroup();
					offer_bundle.semantics_ = "BUNDLE";
					for (size_t i = 1; i < items.size(); ++i) 
					{
						offer_bundle.content_names_.emplace_back(items[i]);
					}
					remote_desc_->content_groups_.push_back(offer_bundle);
				}
			}
			else if (field.find_first_of("m=") == 0) {
				std::vector<std::string> items;
				rtc::tokenize(field, ' ', &items);
				if (items.size() <= 2) {
					RTC_LOG(LS_WARNING) << "parse m= failed: " << field;
					return -1;
				}
				//ContentInfo info(libice::MediaProtocolType::kRtp);
				//info.mid = mid;
				// m=audio/video
				mid = items[0].substr(2);
				
				if (mid == "audio") {
					//   const std::string& name,
					//MediaProtocolType type,
					
					//content_info.type
					//remote_desc_->AddContent(mid, libice::MediaProtocolType::kRtp,  std::move(audio_content));
					//audio_content.reset(remote_desc_->GetContentByName(mid)->media_description());
					audio_td.content_name = mid;
					audio_content_info.name = mid;
				}
				else if (mid == "video") {
					//remote_desc_->AddContent(mid, libice::MediaProtocolType::kRtp, std::move(video_content));
					video_td.content_name = mid;
					video_content_info.name = mid;
					video_pt_ = std::atoi(items[3].c_str());
				}
			}
			
			if ("audio" == mid) {
				
				if (!ParseCandidates(audio_content.get(), field, all_candidate)) {
					RTC_LOG(LS_WARNING) << "parse candidate failed: " << field;
					return -1;
				}

				if (!ParseTransportInfo(&audio_td.description, field)) {
					RTC_LOG(LS_WARNING) << "parse transport info failed: " << field;
					return -1;
				}
			}
			else if ("video" == mid) {
				if (!ParseCandidates(video_content.get(), field, all_candidate)) {
					RTC_LOG(LS_WARNING) << "parse candidate failed: " << field;
					return -1;
				}

				if (!ParseTransportInfo(&video_td.description, field)) {
					RTC_LOG(LS_WARNING) << "parse transport info failed: " << field;
					return -1;
				}
			}
		}
		/*
		
		if (video_content) {
			auto video_codecs = video_content->codecs();
			if (!video_codecs.empty()) {
				video_pt_ = video_codecs[0]->id;
			}
		
			if (video_codecs.size() > 1) {
				video_rtx_pt_ = video_codecs[1]->id;
			}
		}
		*/



		audio_content_info.description_ = std::move(audio_content);
		video_content_info.description_ = std::move(video_content);
		remote_desc_->contents_.push_back(audio_content_info);
		remote_desc_->contents_.push_back(video_content_info);
		remote_desc_->transport_infos_.push_back(audio_td);
		remote_desc_->transport_infos_.push_back(video_td);

		

		transport_controller_->set_remote_sdp(remote_desc_.get());
		for (const libice::Candidate & candidate:all_candidate)
		{
			transport_controller_->set_remote_candidate(candidate);
		}
		
		return 0;
	}
	std::string p2p_peer_connection::create_answer(const RTCOfferAnswerOptions & options, const std::string & stream_id)
	{
		//const uint64_t k_year_in_ms = 365 * 24 * 3600 * 1000L;
		//libice::IceParameters ice_param = libice::IceCredentialsIterator::CreateRandomIceCredentials();
		std::string cname = rtc::CreateRandomString(16);
		//rtc::KeyParams key_params;
		//RTC_LOG(LS_INFO) << "dtls enabled, key type: " << key_params.type();
		//rtc::scoped_refptr<rtc::RTCCertificate> certificate = rtc::RTCCertificateGenerator::GenerateCertificate(key_params,
		//	k_year_in_ms);
		////SSLFingerprint
		//if (certificate)
		//{
		//	rtc::RTCCertificatePEM pem = certificate->ToPEM();
		//	RTC_LOG(LS_INFO) << "rtc certificate: \n" << pem.certificate();
		//	//certificate_ = certificate;
		//}
		//else
		//{
		//	RTC_LOG(LS_WARNING) << " open  certificate failed !!!\n";
		//}
		local_desc_ = std::make_unique <   SessionDescription >(/*webrtc::SdpType::kAnswer*/);
		if (options.send_audio || options.recv_audio) {
			auto audio_content = std::make_unique<AudioContentDescription>();
			//AudioContentDescription audio_content;
			audio_content->direction_ = (GetDirection(options.send_audio, options.recv_audio));
			audio_content->rtcp_mux_ = (options.use_rtcp_mux);
			
			libice::TransportInfo  transport_info;
			transport_info.content_name = "audio";
			transport_info.description.ice_pwd = ice_param_.pwd;
			transport_info.description.ice_ufrag = ice_param_.ufrag;
			transport_info.description.identity_fingerprint = rtc::SSLFingerprint::CreateFromCertificate(*certificate_);
			transport_info.description.connection_role = libice::CONNECTIONROLE_ACTIVE;
			local_desc_->transport_infos_.emplace_back(transport_info);
			ContentInfo  content;// = new ContentInfo();
			content.type = libp2p_peerconnection::MediaProtocolType::kRtp;
			content.name = transport_info.content_name;
			
			libmedia_transfer_protocol::AudioCodec audio_codec;
			audio_codec.id = 111;
			audio_codec.name = "opus";
			audio_codec.channels = 2;
			
			audio_codec.clockrate = 48000;
			// a=fmtp:111 minptime=10;useinbandfec=1
			audio_codec.params.insert(std::make_pair("minptime", "10"));
			audio_codec.params.insert(std::make_pair("useinbandfec", "1"));

			// rtcp-fb:111  transport-cc 
			libmedia_transfer_protocol::FeedbackParam feedbackparam;
			feedbackparam.id_ = "transport-cc";
			//feedbackparam.param_ = "";
			audio_codec.feedback_params.Add(feedbackparam);
			audio_content->codecs_.push_back(audio_codec);
			// 如果发送音频，需要创建stream
			if (options.send_audio) {
				libmedia_transfer_protocol::StreamParams audio_stream;
				audio_stream.id = rtc::CreateRandomString(16);
				audio_stream.stream_ids_.emplace_back(stream_id); //{ std::to_string(stream_id) } ;
				audio_stream.cname = cname;
				local_audio_ssrc_ = rtc::CreateRandomId();
				audio_stream.ssrcs.push_back(local_audio_ssrc_);
				
				audio_content->send_streams_.emplace_back(audio_stream);
			}
			content.description_ = std::move((audio_content));
			local_desc_->contents_.push_back((content));
		}

		if (options.send_video || options.recv_video) {
			auto video_content = std::make_unique<VideoContentDescription>();
			video_content->direction_ = (GetDirection(options.send_video, options.recv_video));
			video_content->rtcp_mux_ = (options.use_rtcp_mux);
			libice::TransportInfo  transport_info;
			transport_info.content_name = "video";
			transport_info.description.identity_fingerprint = rtc::SSLFingerprint::CreateFromCertificate(*certificate_);
			transport_info.description.ice_pwd = ice_param_.pwd;
			transport_info.description.ice_ufrag = ice_param_.ufrag;
			transport_info.description.connection_role = libice::CONNECTIONROLE_ACTIVE;
			local_desc_->transport_infos_.emplace_back(transport_info);

			ContentInfo  content;// = new ContentInfo();// (libice::MediaProtocolType::kRtp);
			content.name = transport_info.content_name;
			
			// 如果发送视频，需要创建stream
			if (options.send_video) {
				std::string id = rtc::CreateRandomString(16);
				libmedia_transfer_protocol::StreamParams video_stream;
				video_stream.id = id;
				video_stream.stream_ids_ .push_back( stream_id );
				video_stream.cname = cname;
				local_video_ssrc_ = rtc::CreateRandomId();
				local_video_rtx_ssrc_ = rtc::CreateRandomId();
				video_stream.ssrcs.push_back(local_video_ssrc_);
				//video_stream.ssrcs.push_back(local_video_rtx_ssrc_);
				// 107
				// 分组
				libmedia_transfer_protocol::SsrcGroup sg;// ("FID", { local_video_ssrc_ , local_video_rtx_ssrc_ });
				sg.semantics = "FID";
				sg.ssrcs = { local_video_ssrc_ , local_video_rtx_ssrc_ };
				//sg.semantics = ;
				//sg.ssrcs.push_back(local_video_ssrc_);
				//sg.ssrcs.push_back(local_video_rtx_ssrc_);
				video_stream.ssrc_groups.push_back(sg);

				video_content->send_streams_.push_back(video_stream);

				// 创建rtx stream
				libmedia_transfer_protocol::StreamParams video_rtx_stream;
				video_rtx_stream.id = id;
				video_rtx_stream.stream_ids_ = { stream_id };
				video_rtx_stream.cname = cname;
				video_rtx_stream.ssrcs.push_back(local_video_rtx_ssrc_);
				video_content->send_streams_.emplace_back(video_rtx_stream);
				libmedia_transfer_protocol::VideoCodec video_codec;
				video_codec.id =  107;
				video_codec.name = "H264";
				video_codec.clockrate = 90000;
				// a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f

				video_codec.params.insert(std::make_pair("level-asymmetry-allowed", "1"));
				video_codec.params.insert(std::make_pair("packetization-mode", "1"));
				video_codec.params.insert(std::make_pair("profile-level-id", "42e01f"));
				//video_codec.params.insert(std::make_pair(std::to_string(video_codec.id), "useinbandfec=1"));

				/*
					a=rtpmap:107 H264/90000
					a=rtcp-fb:107 goog-remb
					a=rtcp-fb:107 transport-cc
					a=rtcp-fb:107 ccm fir
					a=rtcp-fb:107 nack
					a=rtcp-fb:107 nack pli
				*/
				libmedia_transfer_protocol::FeedbackParam feedbackparam;
				feedbackparam.id_ = "goog-remb";
				//feedbackparam.param_ = "transport-cc";
				video_codec.feedback_params.Add(feedbackparam);
				feedbackparam.id_ = "transport-cc"; 
				video_codec.feedback_params.Add(feedbackparam); 
				feedbackparam.id_ = "ccm";
				feedbackparam.param_ = "fir";
				video_codec.feedback_params.Add(feedbackparam); 
				feedbackparam.id_ = "nack";
				feedbackparam.param_ = "";
				video_codec.feedback_params.Add(feedbackparam); 
				feedbackparam.id_ = "nack";
				feedbackparam.param_ = "pli";
				video_codec.feedback_params.Add(feedbackparam);  

				libmedia_transfer_protocol::VideoCodec video_rtx_codec;
				video_rtx_codec.id = 99;
				video_rtx_codec.name = "rtx";
				video_rtx_codec.clockrate = 90000;
				/*
					a=rtpmap:99 rtx/90000
					a=fmtp:99 apt=107
				*/
				video_rtx_codec.id = 99;
				video_rtx_codec.params.insert(std::make_pair("apt", std::to_string(video_codec.id)));
				video_content->codecs_.push_back(video_codec);
				video_content->codecs_.push_back(video_rtx_codec);
			//	CreateVideoSendStream(video_content.get());
			}
			content.description_ = (std::move(video_content));
			local_desc_->contents_.push_back((content));
		}

		// 创建BUNDLE
		if (options.use_rtp_mux) {
			ContentGroup answer_bundle;// ("BUNDLE");
			answer_bundle.semantics_ = "BUNDLE";
			//for (auto content : local_desc_->contents())
			for (size_t i = 0; i <  local_desc_->contents_.size(); ++i)
			{
				answer_bundle.content_names_.emplace_back(local_desc_->contents_[i].name);
				//answer_bundle.AddContentName(content.mid());
			}

			if (!answer_bundle.content_names_.empty()) {
				local_desc_->content_groups_ .emplace_back (answer_bundle);
			}
		}
		{
			context_->network_thread()->PostTask(RTC_FROM_HERE, [this]() {
				RTC_DCHECK_RUN_ON(context_->network_thread());
				libmedia_transfer_protocol::RtpRtcpInterface::Configuration   config;
				config.clock = webrtc::Clock::GetRealTimeClock();
				config.local_media_ssrc = local_video_ssrc_;
				transport_send_ = std::make_unique<libmedia_transfer_protocol::RtpTransportControllerSend>(
					config.clock, nullptr/*rtp_rtcp_impl_*/, task_queue_factory_.get());
				config.transport_feedback_callback = transport_send_.get();
				transport_send_->SignalTargetTransferRate.connect(this, &p2p_peer_connection::OnTragetTransferRate);
				config.bandwidth_callback = this;// transport_send_.get();
			//	transport_send_->SignalOnNetworkInfo();
				rtp_rtcp_impl_ = std::make_unique<libmedia_transfer_protocol::ModuleRtpRtcpImpl>(config);
			});
		}

		transport_controller_->set_local_sdp(local_desc_.get(), certificate_);
		
		return local_desc_->ToString();
		return std::string();
	}
	void p2p_peer_connection::IceTransportStateChanged_n(libice::IceTransportInternal * transport)
	{
		//libice::IceConnectionState::kIceConnectionConnected;
		if (transport->GetState() == libice::IceTransportState::STATE_COMPLETED)
		{
			ice_state = true;
			transport_send_->OnNetworkOk(ice_state);
		}
		else if (transport->GetState() == libice::IceTransportState::STATE_FAILED)
		{
			ice_state = false;
			transport_send_->OnNetworkOk(ice_state);
		}
	}
	void p2p_peer_connection::OnRtcpPacketReceived_n(rtc::CopyOnWriteBuffer * packet, int64_t packet_time_us)
	{
		if (rtp_rtcp_impl_)
		{
			//signalie_thread_->PostTask();
			rtc::CopyOnWriteBuffer   bufer(*packet);
			context_->signaling_thread()->PostTask(/*webrtc::ToQueuedTask(signaling_thread_safety_.flag(),*/RTC_FROM_HERE,  
				[this, packet_ = std::move(bufer) ]() {
				RTC_DCHECK_RUN_ON(context_->signaling_thread());
				rtp_rtcp_impl_->IncomingRtcpPacket(packet_.cdata(), packet_.size());
			});
		}
	}
	void p2p_peer_connection::OnNetworkInfo(const libmedia_transfer_protocol:: ReportBlockList&  reportblocks, int64_t rtt_ms, int64_t now_ms)
	{
		if (transport_send_)
		{
			transport_send_->OnRttUpdate(rtt_ms, webrtc::Timestamp::Millis(now_ms));
			transport_send_->OnReceivedRtcpReceiverReportBlocks(reportblocks, now_ms);
			//std::stringstream cmd;
			//cmd << " rtt: " << rtt_ms << ", now_ms : " << now_ms <<"\r\n";
			//for (const libmedia_transfer_protocol::RTCPReportBlock & reportblock : reportblocks)
			//{
			//	cmd << reportblock.ToString();
			//}
		//	RTC_LOG(LS_INFO) << cmd.str();
			
		}
	}
	void   p2p_peer_connection::SendVideoEncode(std::shared_ptr<libmedia_codec::EncodedImage> encoded_image)
	{
	//	RTC_LOG_F(LS_INFO) << "";


		// 视频的频率90000, 1s中90000份 1ms => 90
		uint32_t rtp_timestamp = encoded_image->Timestamp() * 90;

	/*	if (video_send_stream_) {
			video_send_stream_->OnSendingRtpFrame(rtp_timestamp,
				frame->capture_time_ms,
				frame->fmt.sub_fmt.video_fmt.idr);
		}
*/
		//RTPVideoHeader::RtpPacketizer::Config config;
#if 1
		libmedia_transfer_protocol::RtpPacketizer::PayloadSizeLimits   lists;
		libmedia_transfer_protocol::RTPVideoHeader   rtp_video_hreader;
		//rtc::Buffer encrypted_video_payload;
		//encrypted_video_payload.SetSize(encoded_image->size());
	//	encrypted_video_payload.SetData(encoded_image->size(), encoded_image->data());
	//	rtp_video_hreader.video_type_header = absl::variant<webrtc::RTPVideoHeaderH264>;
		webrtc::RTPVideoHeaderH264  h;
		h.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
		rtp_video_hreader.video_type_header = h;
		std::unique_ptr<libmedia_transfer_protocol::RtpPacketizer> packetizer = 
			libmedia_transfer_protocol::RtpPacketizer::Create(
				libmedia_codec::kVideoCodecH264, rtc::ArrayView<const uint8_t>(encoded_image->data(), encoded_image->size()),
			lists, rtp_video_hreader);
#else 

		webrtc::RtpPacketizer::Config config;
		auto packetizer = webrtc::RtpPacketizer::Create(webrtc::kVideoCodecH264,
			rtc::ArrayView<const uint8_t>((uint8_t*)frame->data[0], frame->data_len[0]),
			config);

#endif 

		while (true) 
		{
			auto  single_packet = std::make_shared<libmedia_transfer_protocol::RtpPacketToSend>(&rtp_header_extension_map_);

		 

			single_packet->SetPayloadType(video_pt_);
			single_packet->SetTimestamp(rtp_timestamp);
			single_packet->SetSsrc(local_video_ssrc_);
			single_packet->ReserveExtension<libmedia_transfer_protocol::TransportSequenceNumber>();
			
			if (!packetizer->NextPacket(single_packet.get())) {
				break;
			}
			int16_t   packet_id = transprot_seq_++;
			single_packet->SetSequenceNumber(video_seq_++);
			single_packet->SetExtension<libmedia_transfer_protocol::TransportSequenceNumber>(packet_id);
			AddPacketToTransportFeedback(packet_id, single_packet.get());
			//if (video_send_stream_) {
			//	video_send_stream_->UpdateRtpStats(single_packet, false, false);
			//}

			//AddVideoCache(single_packet);
			// 发送数据包
			// TODO, transport_name此处写死，后面可以换成变量
			SendPacket("audio",  single_packet.get() );
			
		}

	}
	void p2p_peer_connection::AddPacketToTransportFeedback(uint16_t transport_seq, 
		 libmedia_transfer_protocol::RtpPacketToSend* packet)
	{
		libmedia_transfer_protocol::RtpPacketSendInfo send_info;
		send_info.transport_sequence_number = transport_seq;
		send_info.rtp_timestamp = packet->Timestamp();
		send_info.length = packet->size();
		send_info.packet_type = packet->packet_type();
		
		if (send_info.packet_type.has_value())
		{
			switch (send_info.packet_type.value())
			{
			case libmedia_transfer_protocol::RtpPacketMediaType::kAudio:
			case libmedia_transfer_protocol::RtpPacketMediaType::kVideo:
			{
				send_info.media_ssrc = packet->Ssrc();
				send_info.rtp_sequence_number = packet->SequenceNumber();
			}
			default:
				break;
			}
		}
		transport_send_->OnAddPacket(send_info);
	}
	void p2p_peer_connection::SendPacket(const std::string & transport_name, libmedia_transfer_protocol::RtpPacketToSend*packet)
	{
		transport_controller_->send_rtp_packet(transport_name,(const char *) packet->data(),
			packet->size());


		rtc::SentPacket sent;
		sent.send_time_ms = rtc::TimeMillis();
		if (auto packet_id = packet->GetExtension<libmedia_transfer_protocol::TransportSequenceNumber>())
		{
			sent.packet_id = *packet_id;
		}

		transport_send_->OnSentPacket(sent);
	}
	void p2p_peer_connection::CreateVideoChannel()
	{
		libmedia_transfer_protocol::MediaConfig  media_config;
		libmedia_transfer_protocol::VideoOptions   options;
		libmedia_transfer_protocol::CryptoOptions   crypto_options;
	/*	libmedia_transfer_protocol::VideoMediaChannel* media_channel = media_engine_->video().CreateMediaChannel(
			  media_config, options, crypto_options,
			video_bitrate_allocator_factory_.get());*/
	}


	void p2p_peer_connection::OnTragetTransferRate(libmedia_transfer_protocol::RtpTransportControllerSend *, const libice::TargetTransferRate & target)
	{
		SignalTargetTransferRate(this, target);
	}

	void p2p_peer_connection::OnReceivedEstimatedBitrate(uint32_t bitrate)
	{
		RTC_LOG(LS_INFO) << "bitrate:" << bitrate;
	}
	void p2p_peer_connection::OnReceivedRtcpReceiverReport(
		const libmedia_transfer_protocol::ReportBlockList& report_blocks,
		int64_t rtt,
		int64_t now_ms)
	{
		//for (const libmedia_transfer_protocol::RTCPReportBlock & reportblock : report_blocks)
		{
			OnNetworkInfo(report_blocks, rtt, now_ms);
		}
	}
}
