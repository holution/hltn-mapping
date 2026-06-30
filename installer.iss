#define MyAppName "HLTN Mapping"
#define MyAppVersion "0.0.2"
#define MyAppPublisher "holution"
#define MyAppURL "https://github.com/holution/hltn-mapping"

[Setup]
AppId={{E1B8F7A3-92C6-4D8A-B15E-7F3A2C8D9E6B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonappdata}\obs-studio\plugins\hltn-mapping
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=.\release
OutputBaseFilename=HLTN_Mapping_v{#MyAppVersion}_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "build\hltn-mapping.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "build\libgcc_s_seh-1.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "build\libstdc++-6.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "build\libwinpthread-1.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "data\locale\en-US.ini"; DestDir: "{userappdata}\obs-studio\plugins\hltn-mapping\data\locale"; Flags: ignoreversion

[Code]
function IsOBSInstalled: Boolean;
begin
  Result := DirExists(ExpandConstant('{commonappdata}\obs-studio'));
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  if not IsOBSInstalled then
  begin
    MsgBox('OBS Studio tidak terdeteksi di sistem ini.' + #13#10 +
           'Silakan install OBS Studio terlebih dahulu dari https://obsproject.com/', mbError, MB_OK);
    Result := False;
  end;
end;
