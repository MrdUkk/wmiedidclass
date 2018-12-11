!include x64.nsh

RequestExecutionLevel admin

VIAddVersionKey "ProductName" "EDID Monitor INFO WMI class"
VIAddVersionKey "CompanyName" "dUkk"
VIAddVersionKey "LegalCopyright" "dUkk"
VIAddVersionKey "FileDescription" "Win32_EDIDMonInfo class based on read-edid project maintained by Matthew Kern (poly-p man)"
VIAddVersionKey "FileVersion" "1.0.0"
VIProductVersion "1.0.0.0"

Name "Win32_EDIDMonInfo"
OutFile "edidmoninfo.exe"

;--------------------------------
Section ""
  ${If} ${RunningX64}
        SetRegView 64
        SetOutPath "$WINDIR\syswow64"
  ${Else}
        SetOutPath "$WINDIR\system32"
  ${Endif}

  File "/oname=$OUTDIR\wbem\edidmoninfo.mof" "edidmoninfo.mof"
  File "/oname=$OUTDIR\edidmoninfo.dll" "Release\edidmoninfo.dll"
  ExecWait 'mofcomp.exe $OUTDIR\wbem\edidmoninfo.mof'
  RegDll "$OUTDIR\edidmoninfo.dll"
SectionEnd

