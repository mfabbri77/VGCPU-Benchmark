$Url = "https://github.com/preshing/cairo-windows/releases/download/with-tee/cairo-windows-1.17.2.zip"
$ZipFile = "deps\cairo.zip"
$DestDir = "deps\cairo"

Write-Host "Downloading Cairo from $Url..."
Invoke-WebRequest -Uri $Url -OutFile $ZipFile

Write-Host "Extracting to $DestDir..."
Expand-Archive -Path $ZipFile -DestinationPath $DestDir -Force

Write-Host "Done."
