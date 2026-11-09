New-Item -ItemType Directory -Force -Path cmake-build-debug | Out-Null
New-Item -ItemType Directory -Force -Path cmake-build-release | Out-Null
New-Item -ItemType Directory -Force -Path build | Out-Null
Set-Content -Path (Resolve-Path cmake-build-debug) -Stream com.dropbox.ignored -Value 1
Set-Content -Path (Resolve-Path cmake-build-release) -Stream com.dropbox.ignored -Value 1
Set-Content -Path (Resolve-Path build) -Stream com.dropbox.ignored -Value 1
