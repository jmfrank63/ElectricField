#define AppVersion GetEnv("APP_VERSION")

[Setup]
AppId={{BE9A36BD-2471-48DF-80FD-A0D189FC4C35}
AppName=Electric Field
AppVersion={#AppVersion}
AppPublisher=Johannes Maria Frank
DefaultDirName={autopf}\ElectricField
DefaultGroupName=Electric Field
UninstallDisplayIcon={app}\ElectricField.exe
SetupIconFile=..\..\assets\ElectricField.ico
OutputBaseFilename=ElectricField_{#AppVersion}_windows-x64_setup
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\..\LICENSE

[Files]
Source: "..\..\zig-out\bin\ElectricField.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Electric Field"; Filename: "{app}\ElectricField.exe"
Name: "{autodesktop}\Electric Field"; Filename: "{app}\ElectricField.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"
