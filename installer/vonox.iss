; Inno Setup script: installs VONOX.vst3 into the standard system VST3 folder.
#ifndef Vst3Dir
  #define Vst3Dir "..\build\plugin\VONOX_artefacts\Release\VST3\VONOX.vst3"
#endif
[Setup]
AppName=VONOX
AppVersion=0.1.0
AppPublisher=VONOX
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
OutputBaseFilename=VONOX-Setup
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
Compression=lzma2
[Files]
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\VONOX.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
