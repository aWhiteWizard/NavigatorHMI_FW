<#
.SYNOPSIS
    在 Docker 容器中交叉编译 NavigatorHMI_FW (RK3562 / aarch64)
.DESCRIPTION
    使用 rk3562-builder-env Docker 镜像 + 迅为 6.1 SDK 卷挂载，在容器内完成编译。
    要点 (2026-08-14):
      - SDK (17.8GB, 不打进镜像) 卷挂载到 /sdk
      - 交叉工具链直接用 SDK 自带 prebuilts (buildroot 相对路径自动解析)
      - Buildroot DL 缓存用 SDK 自带 /sdk/buildroot/dl
.PARAMETER Help
    显示此帮助信息
.PARAMETER Target
    编译目标: all (默认, buildroot+kernel+uboot), buildroot, kernel(同 linux), uboot, qt, rootfs, image
.PARAMETER Jobs
    并行编译线程数 (默认: 8)
.PARAMETER Clean
    清理构建目录后重新编译
.PARAMETER DockerImage
    指定使用的 Docker 镜像
    (默认: swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.0-ubuntu20)
.PARAMETER SdkPath
    迅为 6.1 SDK 路径 (默认: D:\workspace\rk3562-sdk\rk3562-linux-6.1)
.PARAMETER Menuconfig
    进入 Linux Kernel / U-Boot / Buildroot 的 menuconfig 交互式配置界面，
    退出时自动保存配置到 hwt/rk3562/ 下。取值: linux, uboot, buildroot
.PARAMETER SkipLogin
    跳过华为云 SWR 登录（已登录时使用）
.EXAMPLE
    .\docker-build-rk3562.ps1
    全部编译 (Buildroot + Kernel + U-Boot)

    .\docker-build-rk3562.ps1 -Target kernel
    只编译 Linux Kernel

    .\docker-build-rk3562.ps1 -Target uboot
    只编译 U-Boot

    .\docker-build-rk3562.ps1 -Target buildroot
    只编译 Buildroot (rootfs, 含 Qt 6.4.3)

    .\docker-build-rk3562.ps1 -Target qt
    用 Buildroot 产出的 host/bin/qmake 交叉编译 Qt 应用

    .\docker-build-rk3562.ps1 -Menuconfig linux
    进入 Linux Kernel menuconfig (退出自动保存到 hwt/rk3562/kernel/.config)

    .\docker-build-rk3562.ps1 -Jobs 16
    16 线程并行编译

    .\docker-build-rk3562.ps1 -Clean
    清理后重新编译
#>

param(
    [switch]$Help,

    [ValidateSet("all", "buildroot", "kernel", "linux", "uboot", "qt", "rootfs", "image")]
    [string]$Target = "all",

    [int]$Jobs = 8,

    [switch]$Clean,

    [string]$DockerImage = "swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/rk3562-builder-env:v1.1-ubuntu20",

    [string]$SdkPath = "D:\workspace\rk3562-sdk\rk3562-linux-6.1",

    [switch]$SkipLogin,

    [ValidateSet("", "linux", "uboot", "buildroot")]
    [string]$Menuconfig = ""
)

# ============================================================
# 处理 -Help 参数
# ============================================================
if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

# ============================================================
# 华为云 SWR 凭据 (优先读用户级环境变量, 2026-08-14 已固化 HKCU\Environment)
# ============================================================
$SWR_AK = $env:SWR_AK
$SWR_SK = $env:SWR_SK
$SWR_Region = if ($env:SWR_REGION) { $env:SWR_REGION } else { "cn-southwest-2" }
$SWR_Domain = "swr.$SWR_Region.myhuaweicloud.com"
$SWR_UserName = "$SWR_Region@$SWR_AK"

# ============================================================
# 脚本位置与路径
# ============================================================
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir    = Join-Path $ProjectRoot "build\rk3562"

# ============================================================
# 参数验证: SDK 路径
# ============================================================
if (-not (Test-Path (Join-Path $SdkPath "buildroot\Makefile"))) {
    Write-Host "错误: 未找到 SDK (缺少 buildroot\Makefile): $SdkPath" -ForegroundColor Red
    Write-Host "请用 -SdkPath 指定迅为 6.1 SDK 解压目录" -ForegroundColor Yellow
    exit 1
}

# ============================================================
# Step 0: 登录华为云 SWR
# ============================================================
if (-not $SkipLogin) {
    if (-not $SWR_AK -or -not $SWR_SK) {
        Write-Host "错误: 未找到 SWR 凭据，请设置用户环境变量 SWR_AK / SWR_SK (或重开终端)" -ForegroundColor Red
        exit 1
    }

    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  Step 0/1: 登录华为云 SWR 镜像仓库"        -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    # 生成 SWR 登录密钥 (HMAC-SHA256)
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
} else {
    Write-Host ">>> 跳过 SWR 登录 (--SkipLogin)" -ForegroundColor Yellow
}

# ============================================================
# Step 1: 清理构建目录
# ============================================================
if ($Clean) {
    Write-Host ">>> 清理构建目录: $BuildDir" -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path "$BuildDir\*" -Recurse -Force
    }
    Write-Host ">>> 清理完成" -ForegroundColor Green
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# ============================================================
# Step 2: Menuconfig 交互式配置
# ============================================================
if ($Menuconfig) {
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  Step: Menuconfig ($Menuconfig)"            -ForegroundColor Cyan
    Write-Host "  退出时将自动保存配置到 hwt/rk3562/"         -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    docker run --rm -it `
        -v "${ProjectRoot}:/workspace" `
        -v "${SdkPath}:/sdk" `
        -e BR2_DL_DIR="/sdk/buildroot/dl" `
        -e FORCE_UNSAFE_CONFIGURE=1 `
        -w /workspace `
        $DockerImage `
        /bin/bash /workspace/build-rk3562.sh $Jobs menuconfig_$Menuconfig

    if ($LASTEXITCODE -ne 0) {
        Write-Host ">>> Menuconfig 失败" -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host ">>> 配置已保存到 hwt/rk3562/$Menuconfig/.config" -ForegroundColor Green
    exit 0
}

# ============================================================
# Step 3: 编译 (Buildroot + Kernel + U-Boot)
# ============================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Step 1/1: 编译 Target=$Target Jobs=$Jobs"    -ForegroundColor Cyan
Write-Host "  SDK:      $SdkPath"                          -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

docker run --rm `
    -v "${ProjectRoot}:/workspace" `
    -v "${SdkPath}:/sdk" `
    -e BR2_DL_DIR="/sdk/buildroot/dl" `
    -e FORCE_UNSAFE_CONFIGURE=1 `
    -w /workspace `
    $DockerImage `
    /bin/bash /workspace/build-rk3562.sh $Jobs $Target

if ($LASTEXITCODE -ne 0) {
    Write-Host ">>> 编译失败 (Target=$Target)" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "============================================" -ForegroundColor Green
Write-Host "  OK 编译成功!"                               -ForegroundColor Green
Write-Host "  产物目录: ${BuildDir}\"                     -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green

if (Test-Path "$BuildDir\kernel") {
    Get-ChildItem "$BuildDir\kernel" | ForEach-Object {
        Write-Host "    - kernel/$($_.Name)" -ForegroundColor White
    }
}
if (Test-Path "$BuildDir\uboot") {
    Get-ChildItem "$BuildDir\uboot" | ForEach-Object {
        Write-Host "    - uboot/$($_.Name)" -ForegroundColor White
    }
}
if (Test-Path "$BuildDir\rootfs") {
    Get-ChildItem "$BuildDir\rootfs" | ForEach-Object {
        Write-Host "    - rootfs/$($_.Name)" -ForegroundColor White
    }
}
