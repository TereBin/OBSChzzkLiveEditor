#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef BuildRoot
  #define BuildRoot "..\release\RelWithDebInfo\OBSChzzkLiveEditor"
#endif
#ifndef OutputRoot
  #define OutputRoot "..\release"
#endif

#define MyAppName "OBSChzzkLiveEditor"
#define MyAppPublisher "OBSChzzkLiveEditor contributors"
#define MyAppExeName "obs64.exe"

[Setup]
AppId={{39D4249E-A2FB-4DBE-90D2-891D70A55AA5}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\OBSChzzkLiveEditor
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#OutputRoot}
OutputBaseFilename=OBSChzzkLiveEditor-{#MyAppVersion}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=force
RestartApplications=no
UninstallDisplayName={#MyAppName}
VersionInfoVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=OBS CHZZK live information editor plugin installer

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#BuildRoot}\bin\64bit\OBSChzzkLiveEditor.dll"; DestDir: "{code:GetObsPath}\obs-plugins\64bit"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#BuildRoot}\bin\64bit\tls\qschannelbackend.dll"; DestDir: "{code:GetObsPath}\bin\64bit\tls"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#BuildRoot}\data\*"; DestDir: "{code:GetObsPath}\data\obs-plugins\OBSChzzkLiveEditor"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
Type: files; Name: "{code:GetObsPath}\obs-plugins\64bit\obs-live-editor.dll"
Type: filesandordirs; Name: "{code:GetObsPath}\data\obs-plugins\obs-live-editor"

[Icons]
Name: "{group}\OBS Studio"; Filename: "{code:GetObsPath}\bin\64bit\{#MyAppExeName}"; WorkingDir: "{code:GetObsPath}\bin\64bit"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Run]
Filename: "{code:GetObsPath}\bin\64bit\{#MyAppExeName}"; Description: "{cm:LaunchProgram,OBS Studio}"; WorkingDir: "{code:GetObsPath}\bin\64bit"; Flags: nowait postinstall skipifsilent runasoriginaluser; Check: IsInstallVerified

[Code]
var
  CachedObsPath: string;
  ObsPathPage: TInputDirWizardPage;
  InstallVerified: Boolean;

function IsObsPath(const Path: string): Boolean;
begin
  Result := FileExists(AddBackslash(Path) + 'bin\64bit\{#MyAppExeName}');
end;

function DetectObsPath(): string;
var
  UninstallCommand: string;
  Candidate: string;
begin
  Result := '';
  if RegQueryStringValue(HKLM32,
      'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio',
      'UninstallString', UninstallCommand) then
  begin
    Candidate := ExtractFileDir(RemoveQuotes(UninstallCommand));
    if IsObsPath(Candidate) then
      Result := Candidate;
  end;

  if Result = '' then
  begin
    Candidate := ExpandConstant('{autopf}\obs-studio');
    if IsObsPath(Candidate) then
      Result := Candidate;
  end;
end;

function GetObsPath(Param: string): string;
begin
  if Assigned(ObsPathPage) and (ObsPathPage.Values[0] <> '') then
    CachedObsPath := RemoveBackslashUnlessRoot(ObsPathPage.Values[0]);
  if CachedObsPath = '' then
    CachedObsPath := DetectObsPath();
  Result := CachedObsPath;
end;

function IsInstallVerified(): Boolean;
begin
  Result := InstallVerified;
end;

procedure InitializeWizard();
var
  DetectedPath: string;
begin
  DetectedPath := DetectObsPath();
  ObsPathPage := CreateInputDirPage(wpWelcome, 'OBS Studio 위치',
    '64비트 OBS Studio가 설치된 폴더를 선택하세요.',
    'bin, data, obs-plugins 폴더가 들어 있는 OBS Studio 최상위 폴더를 지정하세요.', False, '');
  ObsPathPage.Add('');
  ObsPathPage.Values[0] := DetectedPath;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID <> ObsPathPage.ID then
    Exit;

  CachedObsPath := RemoveBackslashUnlessRoot(ObsPathPage.Values[0]);
  if not IsObsPath(CachedObsPath) then
  begin
    MsgBox('선택한 폴더에서 bin\64bit\obs64.exe를 찾지 못했습니다.'#13#10 +
      '64비트 OBS Studio 최상위 폴더를 다시 선택해 주세요.', mbError, MB_OK);
    Result := False;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then
    Exit;

  InstallVerified :=
    FileExists(AddBackslash(CachedObsPath) + 'obs-plugins\64bit\OBSChzzkLiveEditor.dll') and
    FileExists(AddBackslash(CachedObsPath) + 'bin\64bit\tls\qschannelbackend.dll') and
    FileExists(AddBackslash(CachedObsPath) + 'data\obs-plugins\OBSChzzkLiveEditor\locale\ko-KR.ini') and
    not FileExists(AddBackslash(CachedObsPath) + 'obs-plugins\64bit\obs-live-editor.dll');
  if not InstallVerified then
    MsgBox('설치 파일 검증에 실패했습니다. OBS Studio를 실행하기 전에 설치를 다시 시도해 주세요.',
      mbError, MB_OK);
end;
