// In the constructor, initialize XeSS
DxvkRtxRenderer::DxvkRtxRenderer(DxvkDevice* device) 
  : m_device(device),
    // other initializations...
    m_xessContext(std::make_unique<XeSSContext>(device)) {
  
  // Initialize XeSS
  m_xessContext->initialize();
  
  // rest of constructor...
}

// In the method where upscaling is performed (likely in dispatchSR or similar)
void DxvkRtxRenderer::dispatchSR(
  Rc<DxvkCommandList> cmdList,
  const Resources::RaytracingOutput& rtOutput) {
  
  // Get current upscaler selection
  const auto srProvider = RtxOptions::Get()->srProvider.getValue();
  
  if (srProvider == SRProvider::XESS && m_xessContext->shouldUse()) {
    // Use XeSS for upscaling
    const float jitterOffset[2] = { m_constants.jitterOffset.x, m_constants.jitterOffset.y };
    
    bool success = m_xessContext->dispatch(
      cmdList,
      rtOutput.m_finalOutput,                    // Input color
      m_resources.m_finalOutput.image,           // Output upscaled
      rtOutput.m_primaryScreenSpaceMotionVector, // Motion vectors
      rtOutput.m_primaryDepth,                   // Depth
      jitterOffset,
      m_constants.renderResolution,
      m_constants.displayResolution);
      
    if (!success) {
      // Fallback to simple scaling if XeSS fails
      // (existing fallback code)
    }
  } else if (srProvider == SRProvider::DLSS) {
    // Existing DLSS code
    // ...
  } else {
    // Existing fallback code
    // ...
  }
}