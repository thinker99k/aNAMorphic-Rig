Set-Content -Path (Resolve-Path cmake-build-debug) -Stream com.dropbox.ignored -Value 1
Set-Content -Path (Resolve-Path cmake-build-release) -Stream com.dropbox.ignored -Value 1
Set-Content -Path (Resolve-Path build) -Stream com.dropbox.ignored -Value 1
