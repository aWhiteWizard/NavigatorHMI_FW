<#
.SYNOPSIS
    构建 Docker 编译镜像并推送到华为云 SWR
.DESCRIPTION
    1. 构建包含源码压缩包的 Docker 镜像
    2. 推送到华为云 SWR 仓库
    3. 后续可用 docker-build.ps1 使用此镜像编译
.PARAMETER Help
    显示此帮助信息
.PARAMETER ImageTag
    镜像标签 (默认: v1.0)
.PARAMETER SkipPush
    跳过推送，只构建本地镜像
.EXAMPLE
    .\docker-push.ps1
    构建并推送镜像

    .\docker-push.ps1 -ImageTag v1.1 -SkipPush
    构建 v1.1 标签的镜像，仅本地使用

    .\docker-push.ps1 -Help
    显示此帮助信息
#>

param(
    [switch]$Help,

    [string]$ImageTag = "v1.1-ubuntu18",
    [switch]$SkipPush
)

# ============================================================
# 处理 -Help 参数
# ============================================================
if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

# Simple tag validation
if ($ImageTag -notmatch '^[\w.-]+$') {
    Write-Host "错误: 无效的镜像标签 '$ImageTag'" -ForegroundColor Red
    Write-Host "标签只能包含字母、数字、下划线、点和横线" -ForegroundColor Yellow
    exit 1
}

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# ============================================================
# 华为云 SWR 配置
# ============================================================
$SWR_AK = "HPUAQUYWVPFHVHWJJGON"
$SWR_SK = "G6vlkEzbjeG4ZspOgK7pYJPm5G5E7DtbbBvBK5HM"
$SWR_Region = "cn-southwest-2"
$SWR_Domain = "swr.$SWR_Region.myhuaweicloud.com"
$SWR_UserName = "$SWR_Region@$SWR_AK"
$SWR_Namespace = "image-linuxenv"
$ImageName = "fw-builder-env"
$FullImageTag = "${SWR_Domain}/${SWR_Namespace}/${ImageName}:${ImageTag}"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  构建 Docker 编译镜像并推送到 SWR"          -ForegroundColor Cyan
Write-Host "  镜像: ${FullImageTag}"                     -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# ============================================================
# Step 1: 登录华为云 SWR
# ============================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Step 1/3: 登录华为云 SWR"                 -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

$hmacsha256 = New-Object System.Security.Cryptography.HMACSHA256
$hmacsha256.Key = [Text.Encoding]::UTF8.GetBytes($SWR_SK)
$hash = $hmacsha256.ComputeHash([Text.Encoding]::UTF8.GetBytes($SWR_AK))
$SWR_Password = -join ($hash | ForEach-Object { "{0:x2}" -f $_ })

$LoginResult = docker login -u $SWR_UserName -p $SWR_Password $SWR_Domain 2>&1
Write-Host $LoginResult

if ($LASTEXITCODE -ne 0) {
    Write-Host ">>> SWR 登录失败，退出" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host ">>> SWR 登录成功" -ForegroundColor Green

# ============================================================
# Step 2: 构建 Docker 镜像
# ============================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Step 2/3: 构建 Docker 镜像"               -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# 源码压缩包路径
$SourceDir = "D:\workspace\image_sources"

# 使用 .devcontainer/Dockerfile 构建
# --provenance=false 禁用 attestation（华为云 SWR 不支持新版 manifest 格式）
# --build-context sources=... 将外部压缩包目录作为构建上下文传入
docker build --provenance=false -t $FullImageTag `
    -f "${ProjectRoot}\.devcontainer\Dockerfile.ubuntu18" `
    --build-context "sources=${SourceDir}" `
    $ProjectRoot

if ($LASTEXITCODE -ne 0) {
    Write-Host ">>> 镜像构建失败，退出" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host ">>> 镜像构建成功: ${FullImageTag}" -ForegroundColor Green

# ============================================================
# Step 3: 推送到华为云 SWR
# ============================================================
if (-not $SkipPush) {
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  Step 3/3: 推送到华为云 SWR"               -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    docker push $FullImageTag

    if ($LASTEXITCODE -ne 0) {
        Write-Host ">>> 推送失败，退出" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host ">>> 推送成功: ${FullImageTag}" -ForegroundColor Green
} else {
    Write-Host ">>> 跳过推送 (--SkipPush)" -ForegroundColor Yellow
}

Write-Host "============================================" -ForegroundColor Green
Write-Host "  ✓ 全部完成!"                               -ForegroundColor Green
Write-Host "  镜像: ${FullImageTag}"                     -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
