#define AppVersionStr GetFileProductVersion("x86_64-w64-mingw32/switchtec.exe")
#define AppVersion GetVersionNumbersString("x86_64-w64-mingw32/switchtec.exe")

[Setup]
AppName=Switchtec Management CLI
AppVersion={#AppVersionStr}
VersionInfoVersion={#AppVersion}
DefaultDirName={commonpf}\Switchtec
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
OutputDir=.
OutputBaseFilename=switchtec-user-{#AppVersionStr}
ChangesEnvironment=true
DisableWelcomePage=no

[Files]
Source: "x86_64-w64-mingw32\switchtec.exe"; DestDir: "{app}"; \
	Check: Is64BitInstallMode
Source: "x86_64-w64-mingw32\libwinpthread-1.dll"; DestDir: "{app}"; \
	Check: Is64BitInstallMode

; Place all x86 files here, first one should be marked 'solidbreak'
Source: "i686-w64-mingw32\switchtec.exe";  DestDir: "{app}"; \
	Check: not Is64BitInstallMode; Flags: solidbreak
Source: "i686-w64-mingw32\libwinpthread-1.dll"; DestDir: "{app}"; \
	Check: not Is64BitInstallMode

Source: "less.exe"; DestDir: "{app}"; Flags: solidbreak

Source: "x86_64-w64-mingw32\switchtec.dll"; DestDir: "{app}\x64"
Source: "x86_64-w64-mingw32\libswitchtec.a"; DestDir: "{app}\x64"
Source: "x86_64-w64-mingw32\libswitchtec.dll.a"; DestDir: "{app}\x64"

Source: "i686-w64-mingw32\switchtec.dll"; DestDir: "{app}\x32"
Source: "i686-w64-mingw32\libswitchtec.a"; DestDir: "{app}\x32"
Source: "i686-w64-mingw32\libswitchtec.dll.a"; DestDir: "{app}\x32"

Source: "..\inc\switchtec\*"; DestDir: "{app}\inc\switchtec"; \
	Flags: ignoreversion recursesubdirs

[Code]
const EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

procedure EnvAddPath(Path: string);
var
    Paths: string;
begin
    { Retrieve current path (use empty string if entry not exists) }
    if not RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', Paths)
    then Paths := '';

    { Skip if string already found in path }
    if Pos(';' + Uppercase(Path) + ';', ';' + Uppercase(Paths) + ';') > 0 then exit;

    { App string to the end of the path variable }
    Paths := Paths + ';'+ Path +';'

    { Overwrite (or create if missing) path environment variable }
    if RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', Paths)
    then Log(Format('The [%s] added to PATH: [%s]', [Path, Paths]))
    else Log(Format('Error while adding the [%s] to PATH: [%s]', [Path, Paths]));
end;

procedure EnvRemovePath(Path: string);
var
    Paths: string;
    P: Integer;
begin
    { Skip if registry entry not exists }
    if not RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', Paths) then
        exit;

    { Skip if string not found in path }
    P := Pos(';' + Uppercase(Path) + ';', ';' + Uppercase(Paths) + ';');
    if P = 0 then exit;

    { Update path variable }
    Delete(Paths, P - 1, Length(Path) + 1);

    { Overwrite path environment variable }
    if RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', Paths)
    then Log(Format('The [%s] removed from PATH: [%s]', [Path, Paths]))
    else Log(Format('Error while removing the [%s] from PATH: [%s]', [Path, Paths]));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
    if CurStep = ssPostInstall
     then EnvAddPath(ExpandConstant('{app}'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
    if CurUninstallStep = usPostUninstall
    then EnvRemovePath(ExpandConstant('{app}'));
end;
