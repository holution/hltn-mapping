$ErrorActionPreference = "Continue"

$src = "$PSScriptRoot\src"
$obs_src = "$PSScriptRoot\..\obs-studio"
$obs_bin = "C:\Program Files\obs-studio\bin\64bit"
$build = "$PSScriptRoot\build"
$mingw = "C:\msys64\mingw64\bin"

Remove-Item -Recurse -Force $build -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $build | Out-Null

$env:Path = "$mingw;$env:Path"

$incs = @(
    "-I$obs_src\libobs",
    "-I$obs_src\libobs\graphics",
    "-I$src"
)

$cxx_flags = @("-O2", "-std=c++17", "-DWIN32", "-D_WINDOWS", "-fPIC")

# Step 1: Generate import library
Write-Host "Step 1: Generating import library..."
$defFile = "$build\obs.def"
$implib = "$build\libobs.dll.a"

$gpp = "$mingw\gendef.exe"
# Use cmd to avoid PS treating stderr as errors
$defContent = cmd /c "$gpp - `"$obs_bin\obs.dll`" 2>nul" | Out-File -FilePath $defFile -Encoding ASCII
if ((-not (Test-Path $defFile)) -or ((Get-Item $defFile).Length -eq 0)) {
    Write-Host "ERROR: gendef did not produce obs.def"
    exit 1
}
Write-Host "  obs.def: $((Get-Item $defFile).Length) bytes"

$null = cmd /c "$mingw\dlltool.exe -d `"$defFile`" -l `"$implib`" -D `"$obs_bin\obs.dll`"" 2>&1
if (-not (Test-Path $implib)) { Write-Host "ERROR: dlltool failed"; exit 1 }
Write-Host "  Import library: $((Get-Item $implib).Length) bytes"

# Step 2: Compile
Write-Host "Step 2: Compiling..."
$objs = @()
$sources = @("mesh.cpp", "mesh-filter.cpp", "editor.cpp", "plugin.cpp")
foreach ($f in $sources) {
    $obj = "$build\$($f).o"
    $args = @("-c", "$src\$f", "-o", $obj) + $cxx_flags + $incs
    $out = & "$mingw\g++.exe" @args 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $f"
        Write-Host $out
        exit 1
    }
    Write-Host "  $f OK"
    $objs += $obj
}

# Step 3: Link
Write-Host "Step 3: Linking..."
$link_args = @(
    "-shared",
    "-o", "$build\hltn-mapping.dll",
    $objs,
    $implib,
    "-static-libgcc",
    "-static-libstdc++",
    "-Wl,--subsystem,windows"
)
$out = & "$mingw\g++.exe" @link_args 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "LINK FAILED:"
    Write-Host $out
    exit 1
}

$dllPath = "$build\hltn-mapping.dll"
if (Test-Path $dllPath) {
    Write-Host "  hltn-mapping.dll: $((Get-Item $dllPath).Length) bytes"
} else {
    Write-Host "ERROR: DLL not created"
    exit 1
}

# Step 4: Runtime DLLs
Write-Host "Step 4: Runtime DLLs..."
$runtime = @(
    "$mingw\libgcc_s_seh-1.dll",
    "$mingw\libstdc++-6.dll",
    "$mingw\libwinpthread-1.dll"
)
foreach ($r in $runtime) {
    Copy-Item $r -Destination $build -Force
    Write-Host "  $(Split-Path $r -Leaf)"
}

Write-Host "`nBUILD SUCCESS!"
Write-Host "Output: $dllPath"
