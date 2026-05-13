Add-Type -AssemblyName System.Drawing

function Create-IconBitmap($size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    
    $bodyBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 52, 120, 198))
    $margin = [int]($size * 0.15)
    $g.FillRectangle($bodyBrush, $margin, [int]($size * 0.2), $size - 2*$margin, [int]($size * 0.75))
    
    $clipBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 40, 90, 160))
    $clipW = [int]($size * 0.3)
    $clipX = [int](($size - $clipW) / 2)
    $g.FillRectangle($clipBrush, $clipX, [int]($size * 0.08), $clipW, [int]($size * 0.2))
    
    $lineBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $lineH = [Math]::Max(1, [int]($size * 0.06))
    for ($i = 0; $i -lt 3; $i++) {
        $ly = [int]($size * (0.4 + $i * 0.15))
        $lx = [int]($size * 0.25)
        $lw = [int]($size * 0.5)
        $g.FillRectangle($lineBrush, $lx, $ly, $lw, $lineH)
    }
    
    $g.Dispose()
    return $bmp
}

$icon16 = Create-IconBitmap 16
$icon32 = Create-IconBitmap 32
$icon48 = Create-IconBitmap 48

$images = @($icon16, $icon32, $icon48)
$imageData = @()

foreach ($img in $images) {
    $pngStream = New-Object System.IO.MemoryStream
    $img.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
    $imageData += ,$pngStream.ToArray()
    $pngStream.Dispose()
}

$icoPath = Join-Path $PSScriptRoot "xclip.ico"
$icoStream = New-Object System.IO.FileStream($icoPath, [System.IO.FileMode]::Create)
$w = New-Object System.IO.BinaryWriter($icoStream)

$w.Write([UInt16]0)
$w.Write([UInt16]1)
$w.Write([UInt16]$images.Count)

$headerSize = 6 + $images.Count * 16
$offset = $headerSize

for ($i = 0; $i -lt $images.Count; $i++) {
    $sz = $images[$i].Width
    $w.Write([byte]($sz -band 0xFF))
    $w.Write([byte]($sz -band 0xFF))
    $w.Write([byte]0)
    $w.Write([byte]0)
    $w.Write([UInt16]1)
    $w.Write([UInt16]32)
    $w.Write([UInt32]$imageData[$i].Length)
    $w.Write([UInt32]$offset)
    $offset += $imageData[$i].Length
}

foreach ($data in $imageData) {
    $w.Write($data)
}

$w.Dispose()
$icoStream.Dispose()

foreach ($img in $images) { $img.Dispose() }

$fileInfo = Get-Item $icoPath
Write-Host "Icon created: $icoPath ($($fileInfo.Length) bytes)"
