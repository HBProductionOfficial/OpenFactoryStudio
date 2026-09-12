; Per-user installer (Inno Setup 6). package.ps1 stages the program and runs:
;   ISCC.exe /DAppVersion=<version> /DStageDir=<staged folder> /DOutputDir=<folder> ofs.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\build\stage"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\build\dist"
#endif

[Setup]
AppId={{6B0F5A42-1C7E-4E3B-9D2A-0F55C3A71E19}
AppName=Open Factory Studio
AppVersion={#AppVersion}
AppVerName=Open Factory Studio {#AppVersion}
AppPublisher=Open Factory Studio contributors
AppPublisherURL=https://github.com/HBProductionOfficial/OpenFactoryStudio
AppSupportURL=https://github.com/HBProductionOfficial/OpenFactoryStudio/issues
AppUpdatesURL=https://github.com/HBProductionOfficial/OpenFactoryStudio/releases
DefaultDirName={localappdata}\Programs\Open Factory Studio
DefaultGroupName=Open Factory Studio
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#StageDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=OpenFactoryStudio-{#AppVersion}-windows-setup
SetupIconFile=..\..\src\app\ofs.ico
UninstallDisplayIcon={app}\ofs.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Open Factory Studio"; Filename: "{app}\ofs.exe"
Name: "{group}\{cm:UninstallProgram,Open Factory Studio}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Open Factory Studio"; Filename: "{app}\ofs.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ofs.exe"; Description: "{cm:LaunchProgram,Open Factory Studio}"; Flags: nowait postinstall skipifsilent
