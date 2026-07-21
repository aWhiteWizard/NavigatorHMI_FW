# 1. 强制控制台输入输出使用 UTF-8，解决中文显示乱码
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::InputEncoding  = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$headers = @{
    "Authorization" = "Bearer sk-J1oifll9QQj0XjCDZhNOPev0Kpogkgk5ki27LyeavZTsGbbf"
    "Content-Type"  = "application/json; charset=utf-8"
}

# 2. 用 Unicode 码点构造中文，避免脚本文件编码影响字符串内容
$hello = [char]0x4F60 + [char]0x597D   # "你好"

$bodyObj = @{
    model    = "kimi-k3"
    messages = @(
        @{
            role    = "user"
            content = $hello
        }
    )
}
$bodyJson = $bodyObj | ConvertTo-Json -Depth 10

# 3. 显式将请求体编码为 UTF-8 字节，防止 Invoke-RestMethod 用错编码
$bodyBytes = [System.Text.Encoding]::UTF8.GetBytes($bodyJson)

try {
    $response = Invoke-RestMethod -Uri "https://api.moonshot.cn/v1/chat/completions" -Method Post -Headers $headers -Body $bodyBytes
    Write-Host "=== OK: API call succeeded ==="
    Write-Host ("Model: " + $response.model)
    Write-Host ("Reply: " + $response.choices[0].message.content)
    Write-Host ("Usage: " + ($response.usage | ConvertTo-Json -Compress))
}
catch {
    Write-Host "=== FAILED ==="
    Write-Host ("Error: " + $_.Exception.Message)
    if ($_.ErrorDetails) {
        Write-Host ("Detail: " + $_.ErrorDetails.Message)
    }
}