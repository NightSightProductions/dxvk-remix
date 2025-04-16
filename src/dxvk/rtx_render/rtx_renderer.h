#include "rtx_xess_wrapper.h"

class DxvkRtxRenderer {
private:
  // Add XeSS context alongside other upscalers
  std::unique_ptr<XeSSContext> m_xessContext;
  
  // rest of the class...
};