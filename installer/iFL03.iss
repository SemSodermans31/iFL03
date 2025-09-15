; Inno Setup script for iFL03
; Builds a 64-bit installer from the contents of ..\x64\Release

#define MyAppName "iFL03"
#define MyAppPublisher "iFL03"

#ifexist "..\\x64\\Release\\ifl03.exe"
  #define MyAppVersion GetFileVersion("..\\x64\\Release\\ifl03.exe")
  #if MyAppVersion == ""
    #undef MyAppVersion
    #define MyAppVersion "0.0.0.0"
  #endif
#else
  #define MyAppVersion "0.0.0.0"
#endif

; Normalize version so the build/increment is always the 4th component
#define _dot1 Pos(".", MyAppVersion)
#define _A Copy(MyAppVersion, 1, _dot1-1)
#define _rest1 Copy(MyAppVersion, _dot1+1, 99)
#define _dot2 Pos(".", _rest1)
#define _B Copy(_rest1, 1, _dot2-1)
#define _rest2 Copy(_rest1, _dot2+1, 99)
#define _dot3 Pos(".", _rest2)
#define _C Copy(_rest2, 1, _dot3-1)
#define _D Copy(_rest2, _dot3+1, 99)

#if _D == "0" && _C != "0"
  #define NormalizedVersion _A+"."+_B+".0."+_C
#else
  #define NormalizedVersion _A+"."+_B+"."+_C+"."+_D
#endif

[Setup]
AppId={{D7F0B7F0-5C7B-4A75-B8A7-3B9C6E8B9E21}
AppName={#MyAppName}
AppVersion={#NormalizedVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\\ifl03.exe
SetupIconFile=..\\icon.ico
LicenseFile=..\\LICENSE
OutputDir=dist
OutputBaseFilename={#MyAppName}-Setup-{#NormalizedVersion}-x64
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
PrivilegesRequired=lowest
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "..\\x64\\Release\\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\\assets\\*"; DestDir: "{app}\\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\\{#MyAppName}"; Filename: "{app}\\ifl03.exe"; WorkingDir: "{app}"
Name: "{group}\\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\\{#MyAppName}"; Filename: "{app}\\ifl03.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\\ifl03.exe"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent


