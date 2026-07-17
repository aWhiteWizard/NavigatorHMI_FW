<#
.SYNOPSIS
    在 Docker 容器中交叉编译 NavigatorHMI_FW 项目 (i.MX6ULL ARM)
.DESCRIPTION
    使用 fw-builder-env Docker 镜像，在容器内完成全部编译，
    无需打开 VS Code Dev Containers。
.PARAMETER Help
    显示此帮助信息
.PARAMETER BuildType
    构建类型: Debug 或 Release (默认: Debug)
.PARAMETER Target
    编译目标: all (默认), linux, uboot, app, linux+app, rootfs, image
.PARAMETER Clean
    清理构建目录后重新编译
.PARAMETER DockerImage
    指定使用的 Docker 镜像 (默认: swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0)
.PARAMETER Jobs
    并行编译线程数 (默认: 4)
.PARAMETER Menuconfig
    进入 Linux 或 U-Boot 的 menuconfig 交互式配置界面，
    退出时自动保存配置到 hwt/ 目录下。
    取值: linux, uboot
.PARAMETER SkipLogin
    跳过华为云 SWR 登录（已登录时使用）
.EXAMPLE
    .\docker-build.ps1
    全部编译（Linux + U-Boot + 应用 + Rootfs）

    .\docker-build.ps1 -Target linux
    只编译 Linux Kernel

    .\docker-build.ps1 -Target uboot
    只编译 U-Boot

    .\docker-build.ps1 -Target app
    只编译应用

    .\docker-build.ps1 -Target linux+app
    编译 Linux + 应用（跳过 U-Boot）

    .\docker-build.ps1 -Target rootfs
    只编译 Rootfs（需先编出应用）

    .\docker-build.ps1 -Target image
    编译应用 + Rootfs + 完整 SD 卡镜像 (sdcard.img)


    .\docker-build.ps1 -Menuconfig linux
    进入 Linux Kernel menuconfig（交互式配置）

    .\docker-build.ps1 -Menuconfig uboot
    进入 U-Boot menuconfig（交互式配置）

    .\docker-build.ps1 -BuildType Release
    Release 模式编译

    .\docker-build.ps1 -Clean
    清理后重新编译

    .\docker-build.ps1 -Jobs 8
    8 线程并行编译

    .\docker-build.ps1 -Help
    显示此帮助信息
#>

param(
    [switch]$Help,

    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [string]$Target = "all",

    [switch]$Clean,

    [string]$DockerImage = "swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0-ubuntu18",

    [int]$Jobs = 4,

    [switch]$SkipLogin,

    [string]$Menuconfig = ""
)

# ============================================================
# 华为云 SWR 配置
# ============================================================
$SWR_AK = "HPUAQUYWVPFHVHWJJGON"
$SWR_SK = "G6vlkEzbjeG4ZspOgK7pYJPm5G5E7DtbbBvBK5HM"
$SWR_Region = "cn-southwest-2"
$SWR_Domain = "swr.$SWR_Region.myhuaweicloud.com"
$SWR_UserName = "$SWR_Region@$SWR_AK"

# ============================================================
# 处理 -Help 参数
# ============================================================
if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

# ============================================================
# 参数验证
# ============================================================
$ValidTargets = @("all", "linux", "uboot", "app", "linux+app", "rootfs", "image")
$ValidMenuconfigs = @("", "linux", "uboot")

if ($Target -and ($ValidTargets -notcontains $Target)) {
    Write-Host "错误: 无效的 -Target 参数 '$Target'" -ForegroundColor Red
    Write-Host "有效值: $($ValidTargets -join ', ')" -ForegroundColor Yellow
    Write-Host ""
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 1
}

if ($Menuconfig -and ($ValidMenuconfigs -notcontains $Menuconfig)) {
    Write-Host "错误: 无效的 -Menuconfig 参数 '$Menuconfig'" -ForegroundColor Red
    Write-Host "有效值: linux, uboot" -ForegroundColor Yellow
    Write-Host ""
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 1
}

# ============================================================
# 脚本位置与路径
# ============================================================
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir    = Join-Path $ProjectRoot "build"

# ============================================================
# Step 0: 登录华为云 SWR
# ============================================================
if (-not $SkipLogin) {
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
# Step 2: 清理构建目录
# ============================================================
if ($Clean) {
    Write-Host ">>> 清理构建目录: $BuildDir" -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path "$BuildDir\*" -Recurse -Force
    }
    Write-Host ">>> 清理完成" -ForegroundColor Green
}

# 确保构建目录存在
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# ============================================================
# Step 3: Menuconfig 交互式配置
# ============================================================
if ($Menuconfig) {
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  Step: Menuconfig ($Menuconfig)"            -ForegroundColor Cyan
    Write-Host "  退出时将自动保存配置到 hwt/$Menuconfig/arch/arm/configs/" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    docker run --rm -it -v "${ProjectRoot}:/workspace" `
        -v "D:\workspace\image_sources:/sources" `
        -w /workspace `
        $DockerImage `
        /bin/bash /workspace/build-linux-uboot.sh 4 menuconfig_${Menuconfig}

    if ($LASTEXITCODE -ne 0) {
        Write-Host ">>> Menuconfig 失败" -ForegroundColor Red
        exit $LASTEXITCODE
    }

    Write-Host ">>> 配置已保存到 hwt/$Menuconfig/arch/arm/configs/" -ForegroundColor Green
    exit 0
}

# ============================================================
# Step 4: 编译全部（Linux Kernel + U-Boot + 应用 + Rootfs）
# ============================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Step 1/1: 编译 Linux Kernel U-Boot Rootfs" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# 创建 Buildroot 下载缓存目录（位于 /sources/buildroot-2019-dl）
# 用环境变量 BR2_DL_DIR 传递给容器
$BrDlDir = "D:\workspace\image_sources\buildroot-2019-dl"
if (-not (Test-Path $BrDlDir)) {
    New-Item -ItemType Directory -Path $BrDlDir -Force | Out-Null
}

# 编译脚本挂载到容器内执行
# BR2_DL_DIR 告诉 Buildroot 使用宿主机缓存的下载包，避免每次重新下载
# 挂载 image_sources 到 /sources，Buildroot DL 缓存保存在 /sources/buildroot-2019-dl
# FORCE_UNSAFE_CONFIGURE=1 允许以 root 用户运行 Buildroot
docker run --rm -v "${ProjectRoot}:/workspace" `
    -v "D:\workspace\image_sources:/sources" `
    -e BR2_DL_DIR="/sources/buildroot-2019-dl" `
    -e FORCE_UNSAFE_CONFIGURE=1 `
    -w /workspace `
    $DockerImage `
    /bin/bash /workspace/build-linux-uboot.sh $Jobs $Target

if ($LASTEXITCODE -ne 0) {
    Write-Host ">>> Linux/U-Boot 编译失败" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "============================================" -ForegroundColor Green
Write-Host "  ✓ 全部编译成功!"                            -ForegroundColor Green
Write-Host "  产物目录: ${BuildDir}\linux\"               -ForegroundColor Green
Write-Host "    内核:  linux/zImage"                      -ForegroundColor Green
Write-Host "    应用:  linux/bin/NavigatorHMI_FW"         -ForegroundColor Green
Write-Host "    U-Boot: uboot/u-boot.imx"                 -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green

# 列出编译产物
Write-Host ">>> 编译产物:" -ForegroundColor White
if (Test-Path "${BuildDir}\linux") {
    Get-ChildItem "${BuildDir}\linux" | ForEach-Object {
        Write-Host "    - linux/$($_.Name)" -ForegroundColor White
    }
}
if (Test-Path "${BuildDir}\linux\bin") {
    Get-ChildItem "${BuildDir}\linux\bin" | ForEach-Object {
        Write-Host "    - linux/bin/$($_.Name)" -ForegroundColor White
    }
}
if (Test-Path "${BuildDir}\uboot") {
    Get-ChildItem "${BuildDir}\uboot" | ForEach-Object {
        Write-Host "    - uboot/$($_.Name)" -ForegroundColor White
    }
}
