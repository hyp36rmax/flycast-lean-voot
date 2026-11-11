param(
    [Parameter(Mandatory)][ValidateSet('Solo-Local','LAN')][string]$Variant,
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$BuildDirectory
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath $SourceDirectory).Path
$output=[IO.Path]::GetFullPath($BuildDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh build directory; existing builds are preserved.'}
$receipt=Get-Content -LiteralPath (Join-Path $source 'SOURCE-RESTORE-RECEIPT.json') -Raw|ConvertFrom-Json
if($receipt.variant -ne $Variant){throw 'The restored source variant does not match the requested build.'}
$arguments=@('-S',$source,'-B',$output,'-G','Visual Studio 18 2026','-A','x64',
    '-DOT_LEAN_RUNTIME=ON','-DFLYCAST_HOSTED_STARTUP=ON','-DUSE_DX11=ON',
    '-DUSE_DX9=OFF','-DUSE_OPENGL=OFF','-DUSE_VULKAN=OFF',
    '-DFLYCAST_VOOT_RENDER_DIAGNOSTICS=OFF','-DFLYCAST_VOOT_GUEST_TRACE=OFF',
    '-DENABLE_LOG=OFF','-DUSE_DISCORD=OFF','-DUSE_BREAKPAD=ON',
    '-DACTIVATE_MULTITHREADED_COMPILATION=ON','-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
    '-DCMAKE_SYSTEM_VERSION=10.0.26100.0')
& cmake @arguments
if($LASTEXITCODE -ne 0){throw 'Runtime source configuration failed.'}
& cmake --build $output --config Release --target flycast --parallel 1 -- /nr:false
if($LASTEXITCODE -ne 0){throw 'Runtime source build failed.'}
Get-FileHash -LiteralPath (Join-Path $output 'Release/flycast.exe') -Algorithm SHA256
