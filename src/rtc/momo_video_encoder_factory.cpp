#include "momo_video_encoder_factory.h"

#include <iostream>

// WebRTC
#include <absl/memory/memory.h>
#include <absl/strings/match.h>
#include <api/video_codecs/sdp_video_format.h>
#include <api/video_codecs/vp9_profile.h>
#include <media/base/codec.h>
#include <media/base/media_constants.h>
#include <media/engine/simulcast_encoder_adapter.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>
#include <rtc_base/logging.h>

#if !defined(__arm__) || defined(__aarch64__) || defined(__ARM_NEON__)
#include <modules/video_coding/codecs/av1/libaom_av1_encoder.h>
#endif

#if defined(__APPLE__)
#include "mac_helper/objc_codec_factory_helper.h"
#endif

#if USE_MMAL_ENCODER
#include "hwenc_mmal/mmal_h264_encoder.h"
#endif
#if USE_JETSON_ENCODER
#include "hwenc_jetson/jetson_video_encoder.h"
#endif
#if USE_NVCODEC_ENCODER
#include "hwenc_nvcodec/nvcodec_h264_encoder.h"
#endif
#if USE_MSDK_ENCODER
#include "hwenc_msdk/msdk_video_encoder.h"
#endif

MomoVideoEncoderFactory::MomoVideoEncoderFactory(
    const MomoVideoEncoderFactoryConfig& config)
    : config_(config) {
#if defined(__APPLE__)
  video_encoder_factory_ = CreateObjCEncoderFactory();
#endif
  if (config.simulcast) {
    auto config2 = config;
    config2.simulcast = false;
    internal_encoder_factory_.reset(new MomoVideoEncoderFactory(config2));
  }
}
std::vector<webrtc::SdpVideoFormat>
MomoVideoEncoderFactory::GetSupportedFormats() const {
  std::vector<webrtc::SdpVideoFormat> supported_codecs;

  // VP8
  if (config_.vp8_encoder == VideoCodecInfo::Type::Software ||
      config_.vp8_encoder == VideoCodecInfo::Type::Jetson ||
      config_.vp8_encoder == VideoCodecInfo::Type::Intel) {
    supported_codecs.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
  }

  // VP9
  if (config_.vp9_encoder == VideoCodecInfo::Type::Software) {
    for (const webrtc::SdpVideoFormat& format : webrtc::SupportedVP9Codecs()) {
      supported_codecs.push_back(format);
    }
  } else if (config_.vp9_encoder == VideoCodecInfo::Type::Jetson ||
             config_.vp9_encoder == VideoCodecInfo::Type::Intel) {
    supported_codecs.push_back(webrtc::SdpVideoFormat(
        cricket::kVp9CodecName,
        {{webrtc::kVP9FmtpProfileId,
          webrtc::VP9ProfileToString(webrtc::VP9Profile::kProfile0)}}));
  }

  // AV1
  if (config_.av1_encoder == VideoCodecInfo::Type::Software ||
      config_.av1_encoder == VideoCodecInfo::Type::Intel) {
    supported_codecs.push_back(webrtc::SdpVideoFormat(cricket::kAv1CodecName));
  }

  // H264
  std::vector<webrtc::SdpVideoFormat> h264_codecs = {
      CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                       webrtc::H264Level::kLevel3_1, "1"),
      CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                       webrtc::H264Level::kLevel3_1, "0"),
      CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline,
                       webrtc::H264Level::kLevel3_1, "1"),
      CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline,
                       webrtc::H264Level::kLevel3_1, "0")};

  if (config_.h264_encoder == VideoCodecInfo::Type::VideoToolbox) {
    // VideoToolbox の場合は video_encoder_factory_ から H264 を拾ってくる
    for (auto format : video_encoder_factory_->GetSupportedFormats()) {
      if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) {
        supported_codecs.push_back(format);
      }
    }
  } else if (config_.h264_encoder == VideoCodecInfo::Type::NVIDIA) {
#if USE_NVCODEC_ENCODER
    // NVIDIA の場合は対応してる場合のみ追加
    if (NvCodecH264Encoder::IsSupported()) {
      for (const webrtc::SdpVideoFormat& format : h264_codecs) {
        supported_codecs.push_back(format);
      }
    }
#endif
  } else if (config_.h264_encoder == VideoCodecInfo::Type::Intel) {
#if USE_MSDK_ENCODER
    // Intel Media SDK の場合は対応してる場合のみ追加
    if (MsdkVideoEncoder::IsSupported(config_.msdk_session, MFX_CODEC_AVC)) {
      for (const webrtc::SdpVideoFormat& format : h264_codecs) {
        supported_codecs.push_back(format);
      }
    }
#endif
  } else if ((config_.h264_encoder == VideoCodecInfo::Type::Software) ||
             config_.h264_encoder != VideoCodecInfo::Type::NotSupported) {
    // その他のエンコーダの場合は手動で追加
    for (const webrtc::SdpVideoFormat& format : h264_codecs) {
      supported_codecs.push_back(format);
    }
  }

  return supported_codecs;
}

std::unique_ptr<webrtc::VideoEncoder>
MomoVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  // hardware_encoder_only == true の場合、Software なコーデックだったら強制終了する
  if (config_.hardware_encoder_only) {
    bool use_software = false;
    if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName) &&
        config_.vp8_encoder == VideoCodecInfo::Type::Software) {
      use_software = true;
    }
    if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName) &&
        config_.vp9_encoder == VideoCodecInfo::Type::Software) {
      use_software = true;
    }
    if (absl::EqualsIgnoreCase(format.name, cricket::kAv1CodecName) &&
        config_.av1_encoder == VideoCodecInfo::Type::Software) {
      use_software = true;
    }
    if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName) &&
        config_.h264_encoder == VideoCodecInfo::Type::Software) {
      use_software = true;
    }

    if (use_software) {
      std::cerr
          << "The software encoder is not available at the current setting."
          << std::endl;
      std::cerr << "Check the list of available encoders by specifying "
                   "--video-codec-engines."
                << std::endl;
      std::cerr
          << "To enable software encoders, specify --hw-mjpeg-decoder=false."
          << std::endl;
      std::exit(1);
    }
  }

  if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)) {
    if (config_.vp8_encoder == VideoCodecInfo::Type::Software) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return webrtc::VP8Encoder::Create();
      });
    }
#if USE_JETSON_ENCODER
    if (config_.vp8_encoder == VideoCodecInfo::Type::Jetson) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<JetsonVideoEncoder>(cricket::VideoCodec(format)));
      });
    }
#endif
#if USE_MSDK_ENCODER
    if (config_.vp8_encoder == VideoCodecInfo::Type::Intel) {
      return WithSimulcast(format, [msdk_session = config_.msdk_session](
                                       const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<MsdkVideoEncoder>(msdk_session, MFX_CODEC_VP8));
      });
    }
#endif
  }

  if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName)) {
    if (config_.vp9_encoder == VideoCodecInfo::Type::Software) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return webrtc::VP9Encoder::Create(cricket::VideoCodec(format));
      });
    }
#if USE_JETSON_ENCODER
    if (config_.vp9_encoder == VideoCodecInfo::Type::Jetson) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<JetsonVideoEncoder>(cricket::VideoCodec(format)));
      });
    }
#endif
#if USE_MSDK_ENCODER
    if (config_.vp9_encoder == VideoCodecInfo::Type::Intel) {
      return WithSimulcast(format, [msdk_session = config_.msdk_session](
                                       const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<MsdkVideoEncoder>(msdk_session, MFX_CODEC_VP9));
      });
    }
#endif
  }

  if (absl::EqualsIgnoreCase(format.name, cricket::kAv1CodecName)) {
#if !defined(__arm__) || defined(__aarch64__) || defined(__ARM_NEON__)
    if (config_.av1_encoder == VideoCodecInfo::Type::Software) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return webrtc::CreateLibaomAv1Encoder();
      });
    }
#endif
#if USE_MSDK_ENCODER
    if (config_.av1_encoder == VideoCodecInfo::Type::Intel) {
      return WithSimulcast(format, [msdk_session = config_.msdk_session](
                                       const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<MsdkVideoEncoder>(msdk_session, MFX_CODEC_AV1));
      });
    }
#endif
  }

  if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) {
#if defined(__APPLE__)
    if (config_.h264_encoder == VideoCodecInfo::Type::VideoToolbox) {
      return WithSimulcast(
          format, [this](const webrtc::SdpVideoFormat& format) {
            return video_encoder_factory_->CreateVideoEncoder(format);
          });
    }
#endif

#if USE_MMAL_ENCODER
    if (config_.h264_encoder == VideoCodecInfo::Type::MMAL) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<MMALH264Encoder>(cricket::VideoCodec(format)));
      });
    }
#endif

#if USE_JETSON_ENCODER
    if (config_.h264_encoder == VideoCodecInfo::Type::Jetson) {
      return WithSimulcast(format, [](const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<JetsonVideoEncoder>(cricket::VideoCodec(format)));
      });
    }
#endif

#if USE_NVCODEC_ENCODER
    if (config_.h264_encoder == VideoCodecInfo::Type::NVIDIA &&
        NvCodecH264Encoder::IsSupported()) {
      return WithSimulcast(
#if defined(__linux__)
          format,
          [cuda_context =
               config_.cuda_context](const webrtc::SdpVideoFormat& format) {
            return std::unique_ptr<webrtc::VideoEncoder>(
                absl::make_unique<NvCodecH264Encoder>(
                    cricket::VideoCodec(format), cuda_context));
          }
#else
          format,
          [](const webrtc::SdpVideoFormat& format) {
            return std::unique_ptr<webrtc::VideoEncoder>(
                absl::make_unique<NvCodecH264Encoder>(
                    cricket::VideoCodec(format)));
          }
#endif
      );
    }
#endif
#if USE_MSDK_ENCODER
    if (config_.h264_encoder == VideoCodecInfo::Type::Intel) {
      return WithSimulcast(format, [msdk_session = config_.msdk_session](
                                       const webrtc::SdpVideoFormat& format) {
        return std::unique_ptr<webrtc::VideoEncoder>(
            absl::make_unique<MsdkVideoEncoder>(msdk_session, MFX_CODEC_AVC));
      });
    }
#endif
  }

  RTC_LOG(LS_ERROR) << "Trying to created encoder of unsupported format "
                    << format.name;
  return nullptr;
}

std::unique_ptr<webrtc::VideoEncoder> MomoVideoEncoderFactory::WithSimulcast(
    const webrtc::SdpVideoFormat& format,
    std::function<std::unique_ptr<webrtc::VideoEncoder>(
        const webrtc::SdpVideoFormat&)> create) {
  if (internal_encoder_factory_) {
    return std::unique_ptr<webrtc::VideoEncoder>(
        new webrtc::SimulcastEncoderAdapter(internal_encoder_factory_.get(),
                                            format));
  } else {
    return create(format);
  }
}
