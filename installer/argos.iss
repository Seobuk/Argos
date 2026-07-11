; Inno Setup 6 script — packages a pre-built dist\Argos folder into a single setup exe.
; CI passes absolute paths via /D (see .github/workflows/release_windows.yml).

#ifndef DistDir
  #define DistDir "dist\Argos"
#endif
#ifndef OutDir
  #define OutDir "dist"
#endif
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
; Fixed AppId — stable across releases so upgrades are detected.
AppId={{8B1A6E2C-3F7D-4A9E-B0C5-2D9F4E6A1C73}
AppName=Argos
AppVersion={#AppVersion}
AppPublisher=Seobuk
DefaultDirName={autopf}\Argos
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\Argos.exe
OutputDir={#OutDir}
OutputBaseFilename=Argos-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "assocstep"; Description: "STEP/IGES 파일을 Argos와 연결"; GroupDescription: "파일 연결:"

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Argos"; Filename: "{app}\Argos.exe"
Name: "{autoprograms}\Argos 제거"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Argos"; Filename: "{app}\Argos.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\Argos.CADModel"; ValueType: string; ValueName: ""; ValueData: "Argos CAD Model"; Flags: uninsdeletekey; Tasks: assocstep
Root: HKA; Subkey: "Software\Classes\Argos.CADModel\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Argos.exe,0"; Tasks: assocstep
Root: HKA; Subkey: "Software\Classes\Argos.CADModel\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Argos.exe"" ""%1"""; Tasks: assocstep
Root: HKA; Subkey: "Software\Classes\.step\OpenWithProgids"; ValueType: string; ValueName: "Argos.CADModel"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocstep
Root: HKA; Subkey: "Software\Classes\.stp\OpenWithProgids"; ValueType: string; ValueName: "Argos.CADModel"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocstep
Root: HKA; Subkey: "Software\Classes\.iges\OpenWithProgids"; ValueType: string; ValueName: "Argos.CADModel"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocstep
Root: HKA; Subkey: "Software\Classes\.igs\OpenWithProgids"; ValueType: string; ValueName: "Argos.CADModel"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocstep

[Run]
Filename: "{app}\Argos.exe"; Description: "{cm:LaunchProgram,Argos}"; Flags: nowait postinstall skipifsilent
