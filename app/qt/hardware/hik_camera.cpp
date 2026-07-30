#include "hik_camera.h"

#include "MvCameraControl.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

using namespace cv;
using namespace std;

namespace {

constexpr int64_t kMaxCameraFramePixels = 4096LL * 4096LL;

string HexError(const string& prefix, int code)
{
    stringstream ss;
    ss << prefix << " failed, nRet=0x" << hex << code;
    return ss.str();
}

string ReadEnvString(const char* name)
{
    char* value = nullptr;
    size_t value_size = 0;
    string result;
    if (_dupenv_s(&value, &value_size, name) == 0 && value != nullptr && value[0] != '\0') {
        result = value;
    }
    free(value);
    return result;
}

string IpToString(unsigned int ip)
{
    stringstream ss;
    ss << ((ip >> 24) & 0xFF) << "."
       << ((ip >> 16) & 0xFF) << "."
       << ((ip >> 8) & 0xFF) << "."
       << (ip & 0xFF);
    return ss.str();
}

bool ParseIpAddress(const string& text, unsigned int* ip)
{
    if (ip == nullptr) {
        return false;
    }

    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;
    char tail = '\0';
    if (sscanf_s(text.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail, 1) != 4) {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }

    *ip = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

string SafeChars(const unsigned char* value, size_t max_size)
{
    size_t length = 0;
    while (length < max_size && value[length] != '\0') {
        ++length;
    }
    return string(reinterpret_cast<const char*>(value), length);
}

bool IsColorPixel(MvGvspPixelType pixel_type)
{
    switch (pixel_type) {
    case PixelType_Gvsp_BGR8_Packed:
    case PixelType_Gvsp_RGB8_Packed:
    case PixelType_Gvsp_BayerGR8:
    case PixelType_Gvsp_BayerRG8:
    case PixelType_Gvsp_BayerGB8:
    case PixelType_Gvsp_BayerBG8:
    case PixelType_Gvsp_BayerGB10:
    case PixelType_Gvsp_BayerGB10_Packed:
    case PixelType_Gvsp_BayerBG10:
    case PixelType_Gvsp_BayerBG10_Packed:
    case PixelType_Gvsp_BayerRG10:
    case PixelType_Gvsp_BayerRG10_Packed:
    case PixelType_Gvsp_BayerGR10:
    case PixelType_Gvsp_BayerGR10_Packed:
    case PixelType_Gvsp_BayerGB12:
    case PixelType_Gvsp_BayerGB12_Packed:
    case PixelType_Gvsp_BayerBG12:
    case PixelType_Gvsp_BayerBG12_Packed:
    case PixelType_Gvsp_BayerRG12:
    case PixelType_Gvsp_BayerRG12_Packed:
    case PixelType_Gvsp_BayerGR12:
    case PixelType_Gvsp_BayerGR12_Packed:
    case PixelType_Gvsp_BayerGR16:
    case PixelType_Gvsp_BayerRG16:
    case PixelType_Gvsp_BayerGB16:
    case PixelType_Gvsp_BayerBG16:
        return true;
    default:
        return false;
    }
}

bool IsMonoPixel(MvGvspPixelType pixel_type)
{
    switch (pixel_type) {
    case PixelType_Gvsp_Mono8:
    case PixelType_Gvsp_Mono10:
    case PixelType_Gvsp_Mono10_Packed:
    case PixelType_Gvsp_Mono12:
    case PixelType_Gvsp_Mono12_Packed:
    case PixelType_Gvsp_Mono14:
    case PixelType_Gvsp_Mono16:
        return true;
    default:
        return false;
    }
}

} // namespace

HikCamera::HikCamera() = default;

HikCamera::~HikCamera()
{
    close();
}

bool HikCamera::open(const string& preferred_ip_override, string* error_message, double exposure_us)
{
    close();

    int ret = MV_CC_Initialize();
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_Initialize", ret);
        }
        return false;
    }
    sdk_initialized_ = true;

    MV_CC_DEVICE_INFO_LIST device_list;
    memset(&device_list, 0, sizeof(device_list));
    ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_list);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_EnumDevices", ret);
        }
        close();
        return false;
    }
    if (device_list.nDeviceNum == 0) {
        if (error_message != nullptr) {
            *error_message = "No Hikrobot camera found. Check MVS connection and camera IP.";
        }
        close();
        return false;
    }

    const string preferred_ip_text = preferred_ip_override.empty()
        ? ReadEnvString("TANKEYE_CAMERA_IP")
        : preferred_ip_override;
    unsigned int preferred_ip = 0;
    const bool has_preferred_ip = ParseIpAddress(preferred_ip_text, &preferred_ip);
    if (!preferred_ip_text.empty() && !has_preferred_ip) {
        cout << "[Camera] Ignoring invalid TANKEYE_CAMERA_IP=" << preferred_ip_text << endl;
    }

    MV_CC_DEVICE_INFO* device_info = nullptr;
    cout << "[Camera] Enumerated devices: " << device_list.nDeviceNum << endl;
    for (unsigned int i = 0; i < device_list.nDeviceNum; ++i) {
        MV_CC_DEVICE_INFO* candidate = device_list.pDeviceInfo[i];
        if (candidate == nullptr) {
            continue;
        }

        if (candidate->nTLayerType == MV_GIGE_DEVICE) {
            const auto& info = candidate->SpecialInfo.stGigEInfo;
            const string ip = IpToString(info.nCurrentIp);
            const string net_ip = IpToString(info.nNetExport);
            const string model = SafeChars(info.chModelName, sizeof(info.chModelName));
            const string serial = SafeChars(info.chSerialNumber, sizeof(info.chSerialNumber));
            cout << "[Camera] Device[" << i << "] GigE model=" << model
                 << " serial=" << serial
                 << " ip=" << ip
                 << " net=" << net_ip
                 << endl;
            if (has_preferred_ip && info.nCurrentIp == preferred_ip) {
                device_info = candidate;
            }
        } else {
            cout << "[Camera] Device[" << i << "] non-GigE type=0x"
                 << hex << candidate->nTLayerType << dec << endl;
        }
    }

    if (device_info == nullptr) {
        device_info = device_list.pDeviceInfo[0];
    }
    if (has_preferred_ip && device_info->nTLayerType == MV_GIGE_DEVICE &&
        device_info->SpecialInfo.stGigEInfo.nCurrentIp != preferred_ip) {
        if (error_message != nullptr) {
            *error_message = "Configured camera IP was not found: " + preferred_ip_text;
        }
        close();
        return false;
    }
    if (device_info->nTLayerType == MV_GIGE_DEVICE) {
        const auto& selected = device_info->SpecialInfo.stGigEInfo;
        cout << "[Camera] Selected GigE model="
             << SafeChars(selected.chModelName, sizeof(selected.chModelName))
             << " serial=" << SafeChars(selected.chSerialNumber, sizeof(selected.chSerialNumber))
             << " ip=" << IpToString(selected.nCurrentIp)
             << " net=" << IpToString(selected.nNetExport)
             << endl;
    }

    ret = MV_CC_CreateHandle(&handle_, device_info);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_CreateHandle", ret);
        }
        close();
        return false;
    }

    ret = MV_CC_OpenDevice(handle_);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_OpenDevice", ret);
        }
        close();
        return false;
    }

    if (device_info->nTLayerType == MV_GIGE_DEVICE) {
        const int packet_size = MV_CC_GetOptimalPacketSize(handle_);
        if (packet_size > 0) {
            MV_CC_SetIntValueEx(handle_, "GevSCPSPacketSize", packet_size);
        }
    }

    ret = MV_CC_SetEnumValue(handle_, "AcquisitionMode", MV_ACQ_MODE_CONTINUOUS);
    if (ret != MV_OK) {
        // Some models expose this as read-only through specific user sets; continue if trigger mode can be forced off.
    }

    ret = MV_CC_SetEnumValue(handle_, "TriggerMode", MV_TRIGGER_MODE_OFF);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_SetEnumValue(TriggerMode)", ret);
        }
        close();
        return false;
    }

    ret = MV_CC_SetBayerCvtQuality(handle_, 1);
    if (ret != MV_OK) {
        // Some devices may reject this before conversion; grabbing can still work.
    }

    if (exposure_us > 0.0 && !setManualExposure(exposure_us, error_message)) {
        close();
        return false;
    }

    ret = MV_CC_StartGrabbing(handle_);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_StartGrabbing", ret);
        }
        close();
        return false;
    }
    grabbing_ = true;
    return true;
}

bool HikCamera::grab(Mat& frame, string* error_message)
{
    if (handle_ == nullptr || !grabbing_) {
        if (error_message != nullptr) {
            *error_message = "Hikrobot camera is not open.";
        }
        return false;
    }

    MV_FRAME_OUT sdk_frame;
    memset(&sdk_frame, 0, sizeof(sdk_frame));
    int ret = MV_OK;
    for (int attempt = 0; attempt < 8; ++attempt) {
        ret = MV_CC_GetImageBuffer(handle_, &sdk_frame, 1000);
        if (ret == MV_OK) {
            break;
        }
        if (ret != MV_E_NODATA) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_GetImageBuffer", ret);
        }
        return false;
    }

    const bool ok = convertFrameToBgr(sdk_frame, frame, error_message);
    const int free_ret = MV_CC_FreeImageBuffer(handle_, &sdk_frame);
    if (free_ret != MV_OK && ok && error_message != nullptr) {
        *error_message = HexError("MV_CC_FreeImageBuffer", free_ret);
        return false;
    }
    return ok;
}

bool HikCamera::setManualExposure(double exposure_us, string* error_message)
{
    if (handle_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Hikrobot camera is not open.";
        }
        return false;
    }
    if (exposure_us <= 0.0) {
        if (error_message != nullptr) {
            *error_message = "ExposureTime must be greater than 0 us.";
        }
        return false;
    }

    int ret = MV_CC_SetEnumValueByString(handle_, "ExposureAuto", "Off");
    if (ret != MV_OK) {
        ret = MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
    }
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_SetEnumValue(ExposureAuto=Off)", ret);
        }
        return false;
    }

    ret = MV_CC_SetFloatValue(handle_, "ExposureTime", static_cast<float>(exposure_us));
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_SetFloatValue(ExposureTime)", ret);
        }
        return false;
    }
    cout << "[Camera] ExposureTime set to " << exposure_us << " us" << endl;
    return true;
}

bool HikCamera::getExposure(double* exposure_us, string* error_message) const
{
    if (exposure_us == nullptr) {
        if (error_message != nullptr) {
            *error_message = "exposure output pointer is null";
        }
        return false;
    }
    if (handle_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Hikrobot camera is not open.";
        }
        return false;
    }

    MVCC_FLOATVALUE value;
    memset(&value, 0, sizeof(value));
    const int ret = MV_CC_GetFloatValue(handle_, "ExposureTime", &value);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_GetFloatValue(ExposureTime)", ret);
        }
        return false;
    }
    *exposure_us = static_cast<double>(value.fCurValue);
    return true;
}

bool HikCamera::autoExposureOnce(double* exposure_us, string* error_message)
{
    if (handle_ == nullptr || !grabbing_) {
        if (error_message != nullptr) {
            *error_message = "Hikrobot camera is not open.";
        }
        return false;
    }

    int ret = MV_CC_SetEnumValueByString(handle_, "ExposureAuto", "Once");
    if (ret != MV_OK) {
        // Many Hikrobot cameras use Off=0, Once=1, Continuous=2.
        ret = MV_CC_SetEnumValue(handle_, "ExposureAuto", 1);
    }
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_SetEnumValue(ExposureAuto=Once)", ret);
        }
        return false;
    }

    for (int i = 0; i < 8; ++i) {
        Mat frame;
        string grab_error;
        if (!grab(frame, &grab_error)) {
            if (error_message != nullptr) {
                *error_message = "Auto exposure failed while grabbing frame: " + grab_error;
            }
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    if (!getExposure(exposure_us, error_message)) {
        return false;
    }

    ret = MV_CC_SetEnumValueByString(handle_, "ExposureAuto", "Off");
    if (ret != MV_OK) {
        ret = MV_CC_SetEnumValue(handle_, "ExposureAuto", 0);
    }
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_SetEnumValue(ExposureAuto=Off)", ret);
        }
        return false;
    }
    if (exposure_us != nullptr && *exposure_us > 0.0) {
        return setManualExposure(*exposure_us, error_message);
    }
    return true;
}

void HikCamera::close()
{
    if (handle_ != nullptr) {
        if (grabbing_) {
            MV_CC_StopGrabbing(handle_);
            grabbing_ = false;
        }
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
    }

    if (sdk_initialized_) {
        MV_CC_Finalize();
        sdk_initialized_ = false;
    }
}

bool HikCamera::isOpen() const
{
    return handle_ != nullptr && grabbing_;
}

bool HikCamera::convertFrameToBgr(const MV_FRAME_OUT& sdk_frame,
                                  Mat& frame,
                                  string* error_message)
{
    const auto& info = sdk_frame.stFrameInfo;
    const int width = static_cast<int>(info.nExtendWidth);
    const int height = static_cast<int>(info.nExtendHeight);
    if (width <= 0 || height <= 0 || sdk_frame.pBufAddr == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Invalid frame returned by Hikrobot SDK.";
        }
        return false;
    }
    const int64_t pixel_count = static_cast<int64_t>(width) * static_cast<int64_t>(height);
    if (pixel_count > kMaxCameraFramePixels) {
        if (error_message != nullptr) {
            *error_message = "Hikrobot frame is larger than the safety limit.";
        }
        return false;
    }

    if (info.enPixelType == PixelType_Gvsp_BGR8_Packed) {
        Mat bgr(height, width, CV_8UC3, sdk_frame.pBufAddr);
        frame = bgr.clone();
        return true;
    }

    if (info.enPixelType == PixelType_Gvsp_RGB8_Packed) {
        Mat rgb(height, width, CV_8UC3, sdk_frame.pBufAddr);
        cvtColor(rgb, frame, COLOR_RGB2BGR);
        return true;
    }

    if (info.enPixelType == PixelType_Gvsp_Mono8) {
        Mat mono(height, width, CV_8UC1, sdk_frame.pBufAddr);
        cvtColor(mono, frame, COLOR_GRAY2BGR);
        return true;
    }

    MvGvspPixelType dst_type = PixelType_Gvsp_Undefined;
    int channels = 0;
    if (IsColorPixel(info.enPixelType)) {
        dst_type = PixelType_Gvsp_BGR8_Packed;
        channels = 3;
    } else if (IsMonoPixel(info.enPixelType)) {
        dst_type = PixelType_Gvsp_Mono8;
        channels = 1;
    }

    if (dst_type == PixelType_Gvsp_Undefined) {
        if (error_message != nullptr) {
            *error_message = "Unsupported Hikrobot pixel type.";
        }
        return false;
    }

    vector<unsigned char> converted(static_cast<size_t>(pixel_count) * static_cast<size_t>(channels));
    MV_CC_PIXEL_CONVERT_PARAM_EX convert_param;
    memset(&convert_param, 0, sizeof(convert_param));
    convert_param.nWidth = info.nExtendWidth;
    convert_param.nHeight = info.nExtendHeight;
    convert_param.pSrcData = sdk_frame.pBufAddr;
    convert_param.nSrcDataLen = static_cast<unsigned int>(info.nFrameLenEx);
    convert_param.enSrcPixelType = info.enPixelType;
    convert_param.enDstPixelType = dst_type;
    convert_param.pDstBuffer = converted.data();
    convert_param.nDstBufferSize = static_cast<unsigned int>(converted.size());

    const int ret = MV_CC_ConvertPixelTypeEx(handle_, &convert_param);
    if (ret != MV_OK) {
        if (error_message != nullptr) {
            *error_message = HexError("MV_CC_ConvertPixelTypeEx", ret);
        }
        return false;
    }

    if (channels == 3) {
        Mat bgr(height, width, CV_8UC3, converted.data());
        frame = bgr.clone();
    } else {
        Mat mono(height, width, CV_8UC1, converted.data());
        cvtColor(mono, frame, COLOR_GRAY2BGR);
    }
    return true;
}
