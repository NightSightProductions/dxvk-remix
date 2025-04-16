@echo off

REM Add code to copy XeSS DLLs
echo Copying Intel XeSS DLLs...
if exist "..\XeSS_SDK_2.0.1\bin" (
  copy "..\XeSS_SDK_2.0.1\bin\libxess.dll" "build\dxvk-remix-install\bin\x64\"
) else (
  echo Warning: XeSS SDK not found at expected location
)

REM Rest of build script