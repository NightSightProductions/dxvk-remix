#include "rtx_xess.h"

#ifdef ENABLE_XESS

#include "rtx_options.h"
#include "rtx_render_helper.h"
#include "../tracy/Tracy.hpp"
#include "../util/log/log.h"

namespace dxvk {

  XeSSContext::XeSSContext(Rc<DxvkDevice> device)
    : m_device(device) {
  }

  XeSSContext::~XeSSContext() {
    destroyContext();
  }

  bool XeSSContext::shouldUse() const {
    return m_initialized && 
           m_supported && 
           RtxOptions::Get()->isSREnabled() && 
           RtxOptions::Get()->srProvider.getValue() == SRProvider::XESS;
  }

  bool XeSSContext::initialize() {
    ScopedCpuProfileZone();
    
    // Early out if already initialized
    if (m_initialized) {
      return true;
    }
    
    // Check if XeSS is available
    XessDeviceCapability deviceCapability = {};
    xess_result_t err = xessDeviceQueryCapability(&deviceCapability);
    
    if (err != XESS_RESULT_SUCCESS) {
      Logger::warn("XeSS: Failed to query device capability. Error code: " + std::to_string(err));
      m_supported = false;
      return false;
    }
    
    // Check if SR is supported
    m_supported = (deviceCapability.supportSR != 0);
    
    if (!m_supported) {
      Logger::warn("XeSS: Super Resolution is not supported on this device");
      return false;
    }
    
    // Initialize Vulkan device context
    VkDevice vkDevice = m_device->handle();
    VkPhysicalDevice vkPhysicalDevice = m_device->adapter()->handle();
    VkInstance vkInstance = m_device->adapter()->instance()->handle();
    
    m_deviceContext.vkDevice = vkDevice;
    m_deviceContext.vkPhysicalDevice = vkPhysicalDevice;
    m_deviceContext.vkInstance = vkInstance;
    
    // Set m_initialized now that we've checked support
    m_initialized = true;
    Logger::info("XeSS: Successfully initialized");
    
    return true;
  }

  xess_quality_settings_t XeSSContext::getCurrentQualitySetting() const {
    return convertQualitySetting();
  }

  xess_quality_settings_t XeSSContext::convertQualitySetting() const {
    // Convert from RTX options quality setting to XeSS quality
    auto qualityLevel = RtxOptions::Get()->getSRQuality();
    
    switch (qualityLevel) {
      case DlssPreset::UltraPerformance:
        return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
      case DlssPreset::Performance:
        return XESS_QUALITY_SETTING_PERFORMANCE;
      case DlssPreset::Balanced:
        return XESS_QUALITY_SETTING_BALANCED;
      case DlssPreset::Quality:
      default:
        return XESS_QUALITY_SETTING_QUALITY;
    }
  }

  bool XeSSContext::createContext(uint32_t renderWidth, uint32_t renderHeight,
                                uint32_t displayWidth, uint32_t displayHeight) {
    ScopedCpuProfileZone();
    
    // Destroy existing context if any
    destroyContext();
    
    // Configure XeSS context
    XessSrCreateInfo createInfo = {};
    createInfo.deviceContext = &m_deviceContext;
    createInfo.deviceType = XESS_DEVICE_TYPE_VULKAN;
    createInfo.quality = convertQualitySetting();
    createInfo.outputWidth = displayWidth;
    createInfo.outputHeight = displayHeight;
    createInfo.inputWidth = renderWidth;
    createInfo.inputHeight = renderHeight;
    createInfo.hdr = false; // Set to true for HDR
    
    // Create XeSS SR context
    xess_result_t err = xessSrCreateContext(&createInfo, &m_xessContext);
    
    if (err != XESS_RESULT_SUCCESS) {
      Logger::err("XeSS: Failed to create SR context. Error code: " + std::to_string(err));
      m_xessContext = nullptr;
      return false;
    }
    
    return true;
  }

  void XeSSContext::destroyContext() {
    if (m_xessContext) {
      xessDestroyContext(m_xessContext);
      m_xessContext = nullptr;
    }
  }

  bool XeSSContext::dispatch(
    const Rc<DxvkCommandList>& cmdList,
    const Rc<DxvkImage>& input, 
    const Rc<DxvkImage>& output,
    const Rc<DxvkImage>& motionVectors,
    const Rc<DxvkImage>& depth,
    const float jitterOffset[2],
    const uint32_t renderResolution[2],
    const uint32_t displayResolution[2]) {
    
    ScopedCpuProfileZone();
    
    if (!m_initialized || !m_supported) {
      return false;
    }
    
    // Create or recreate context if needed
    if (m_xessContext == nullptr) {
      if (!createContext(renderResolution[0], renderResolution[1], 
                       displayResolution[0], displayResolution[1])) {
        return false;
      }
    }
    
    // Get Vulkan command buffer
    VkCommandBuffer vkCommandBuffer = cmdList->getCmdBuffer(DxvkCmdBuffer::ExecBuffer);
    
    // Set up XeSS parameters
    XessSrDispatchInfo dispatchInfo = {};
    
    // Input color buffer
    XessImage inputColor = {};
    inputColor.image = static_cast<void*>(input->handle());
    inputColor.width = renderResolution[0];
    inputColor.height = renderResolution[1];
    inputColor.format = XESS_FORMAT_R16G16B16A16_FLOAT; // Adjust based on actual format
    dispatchInfo.colorInput = &inputColor;
    
    // Motion vectors
    XessImage inputMotionVectors = {};
    inputMotionVectors.image = static_cast<void*>(motionVectors->handle());
    inputMotionVectors.width = renderResolution[0];
    inputMotionVectors.height = renderResolution[1];
    inputMotionVectors.format = XESS_FORMAT_R16G16_FLOAT; // Adjust based on actual format
    dispatchInfo.mvInput = &inputMotionVectors;
    
    // Depth buffer (optional)
    if (depth != nullptr) {
      XessImage inputDepth = {};
      inputDepth.image = static_cast<void*>(depth->handle());
      inputDepth.width = renderResolution[0];
      inputDepth.height = renderResolution[1];
      inputDepth.format = XESS_FORMAT_D32_FLOAT; // Adjust based on actual format
      dispatchInfo.depthInput = &inputDepth;
    }
    
    // Output
    XessImage outputImage = {};
    outputImage.image = static_cast<void*>(output->handle());
    outputImage.width = displayResolution[0];
    outputImage.height = displayResolution[1];
    outputImage.format = XESS_FORMAT_R16G16B16A16_FLOAT; // Adjust based on actual format
    dispatchInfo.colorOutput = &outputImage;
    
    // Jitter
    dispatchInfo.jitterOffsetX = jitterOffset[0];
    dispatchInfo.jitterOffsetY = jitterOffset[1];
    
    // Command buffer
    dispatchInfo.commandBuffer = vkCommandBuffer;
    
    // Dispatch XeSS
    xess_result_t err = xessSrDispatch(m_xessContext, &dispatchInfo);
    
    if (err != XESS_RESULT_SUCCESS) {
      Logger::err("XeSS: Failed to dispatch. Error code: " + std::to_string(err));
      return false;
    }
    
    return true;
  }
}

#endif // ENABLE_XESS