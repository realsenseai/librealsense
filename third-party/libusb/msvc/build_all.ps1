$toolsets = "v120", "v140", "v141", "v142", "v143"
$platforms = "Win32", "x64", "ARM", "ARM64"
$configurations = "Debug", "Release"

foreach ($toolset in $toolsets) {
    foreach ($plat in $platforms) {
        if (("v120", "v140").contains($toolset) -and $plat -eq "ARM64") {
            # VS2013,VS2015 don't support arm64
            write-host ">>> PlatformToolset=$toolset,Platform=$plat SKIP"
            continue
        }
        foreach ($conf in $configurations) {
            write-host ">>> PlatformToolset=$toolset,Platform=$plat,Configuration=$conf"
            msbuild -m -v:m -p:PlatformToolset=$toolset,Platform=$plat,Configuration=$conf $PSScriptRoot\libusb.sln
        }
    }
}