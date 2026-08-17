<#
.SYNOPSIS
    构建 RK3562 Docker 编译镜像并推送到华为云 SWR
.DESCRIPTION
    1. 构建包含编译依赖的 rk3562-builder-env 镜像 (SDK 源码不打进镜像, 卷挂载)
    2. 推送到华为云 SWR 仓库 (image-linuxenv)
    3. 后续可用 docker-build-rk3562.ps1 使用此镜像编译
.PARAMETER Help
    显示此帮助信息
.PARAMETER ImageTag
    镜像标签 (默认: v1.0-ubuntu20)
.PARAMETER SkipPush
    跳过推送，只构建本地镜像
.EXAMPLE
    .\docker-push-rk3562.ps1
    构建并推送镜像

    .\docker-push-rk3562.ps1 -ImageTag v1.1 -SkipPush
    构建 v1.1 标签的镜像，仅本地使用

    .\docker-push-rk3562.ps1 -Help
    显示此帮助信息
#>

param(
    [switch]$Help,

    [string]$ImageTag = "v1.1-ubuntu20",
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
# 华为云 SWR 配置（2026-08-14 起优先读用户级环境变量 SWR_AK/SWR_SK/SWR_REGION/SWR_NAMESPACE；
# 已固化到 HKCU\Environment，避免脚本明文与变量两处不同步）
# ============================================================
$SWR_AK = $env:SWR_AK
$SWR_SK = $env:SWR_SK
$SWR_Region = if ($env:SWR_REGION) { $env:SWR_REGION } else { "cn-southwest-2" }
$SWR_Domain = "swr.$SWR_Region.myhuaweicloud.com"
$SWR_UserName = "$SWR_Region@$SWR_AK"
$SWR_Namespace = if ($env:SWR_NAMESPACE) { $env:SWR_NAMESPACE } else { "image-linuxenv" }
if (-not $SWR_AK -or -not $SWR_SK) {
    Write-Host "❌ 未找到 SWR 凭据：请先设置用户环境变量 SWR_AK / SWR_SK（或重开终端使环境变量生效）" -ForegroundColor Red
    exit 1
}
$ImageName = "rk3562-builder-env"
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

# 使用 .devcontainer/Dockerfile.rk3562 构建
# --provenance=false 禁用 attestation（华为云 SWR 不支持新版 manifest 格式）
# SDK 源码(17.8GB)不打进镜像, 编译时用 docker-build-rk3562.ps1 卷挂载
docker build --provenance=false -t $FullImageTag `
    -f "${ProjectRoot}\.devcontainer\Dockerfile.rk3562" `
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
