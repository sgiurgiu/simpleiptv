param(
    [string]
    $libMpvDir = "E:\projects\mpv",
    [string]
    $libMpvIncludeDir = "E:\projects\mpv\include"
)


# Invokes a Cmd.exe shell script and updates the environment.
function Invoke-CmdScript {
  param(
    [String] $scriptName
  )
  $cmdLine = """$scriptName"" $args & set"
  & $Env:SystemRoot\system32\cmd.exe /c $cmdLine |
  select-string '^([^=]*)=(.*)$' | foreach-object {
    $varName = $_.Matches[0].Groups[1].Value
    $varValue = $_.Matches[0].Groups[2].Value
    set-item Env:$varName $varValue
  }
}

# Stop the script when a cmdlet or a native command fails
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

Invoke-CmdScript "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

$packagesDir = ".\packages"

Remove-Item -LiteralPath $packagesDir -Force -Recurse -ErrorAction Ignore
New-Item -ItemType Directory -Force -Path $packagesDir


echo "Using lib mpv directory: $libMpvDir"
echo "Using lib mpv include directory: $libMpvIncludeDir"

set-item Env:libMpvDir $libMpvDir    
set-item Env:libMpvIncludeDir $libMpvIncludeDir
$version = git describe --tags
set-item Env:SIMPLEIPTV_VERSION $version
echo "Building Simple IPTV Version $version"
set-item Env:DISTRIBUTION "windows"

cmake --workflow --preset=release-windows

Copy-Item -Path "$buildDir\*.msi" -Destination $packagesDir -Verbose
echo "Copying $buildDir\*.msi to $packagesDir"


