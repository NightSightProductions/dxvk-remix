#pragma once

#ifdef ENABLE_XESS

#include "../util/util_env.h"
#include "../util/util_string.h"
#include "../util/util_vector.h"
#include "../util/config/config.h"

#include "rtx_types.h"
#include "rtx_options.h"

// Include XeSS headers
#include <xess/xess.h>
#include <xess/xess_vk.h>

namespace dxvk {
  
  // Super Resolution Provider enum extension
  enum class SRProvider {
    DLSS,
    XESS,
    None
  };
  
  // Wrapper class for Intel XeSS
  class XeSSContext {
  public:
    XeSSContext(Rc<DxvkDevice> device);
    ~XeSSContext();
    
    // Initialize XeSS
    bool initialize();
    
    // Check if XeSS is supported on this system
    bool isSupported() const { return m_supported; }
    
    // Evaluate whether XeSS can/should be used
    bool shouldUse() const;
    
    // Get current XeSS quality setting
    xess_quality_settings_t getCurrentQualitySetting() const;
    
    // Dispatch XeSS
    bool dispatch(
      const Rc<DxvkCommandList>& cmdList,
      const Rc<DxvkImage>& input, 
      const Rc<DxvkImage>& output,
      const Rc<DxvkImage>& motionVectors,
      const Rc<DxvkImage>& depth,
      const float jitterOffset[2],
      const uint32_t renderResolution[2],
      const uint32_t displayResolution[2]);
    
  private:
    Rc<DxvkDevice> m_device;
    
    xess_context_handle_t m_xessContext = nullptr;
    XessVkDeviceContext m_deviceContext = {};
    
    bool m_initialized = false;
    bool m_supported = false;
    
    // Helper function to convert quality setting from RTX options to XeSS
    xess_quality_settings_t convertQualitySetting() const;
    
    // Create a new XeSS context with current settings
    bool createContext(uint32_t renderWidth, uint32_t renderHeight, 
                      uint32_t displayWidth, uint32_t displayHeight);
    
    // Destroy current context
    void destroyContext();
  };
}

#endif // ENABLE_XESS