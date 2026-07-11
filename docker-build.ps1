<#
.SYNOPSIS
    在 Docker 容器中交叉编译 NavigatorHMI_FW 项目 (i.MX6ULL ARM)
.DESCRIPTION
    使用 fw-builder-env Docker 镜像，在容器内完成全部编译，
    无需打开 VS Code Dev Containers。
.PARAMETER BuildType
    构建类型: Debug 或 Release (默认: Debug)
.PARAMETER Target
    编译目标: all (默认), linux, uboot, app, linux+app
.PARAMETER Clean
    清理构建目录后重新编译
.PARAMETER DockerImage
    指定使用的 Docker 镜像 (默认: swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0)
.PARAMETER Jobs
    并行编译线程数 (默认: 4)
.EXAMPLE
    .\docker-build.ps1
    全部编译（Linux + U-Boot + 应用）

    .\docker-build.ps1 -Target linux
    只编译 Linux Kernel

    .\docker-build.ps1 -Target uboot
    只编译 U-Boot

    .\docker-build.ps1 -Target app
    只编译应用

    .\docker-build.ps1 -Target linux+app
    编译 Linux Kernel + 应用（跳过 U-Boot）

    .\docker-build.ps1 -BuildType Release
    Release 模式编译

    .\docker-build.ps1 -Clean
    清理后重新编译

    .\docker-build.ps1 -Jobs 8
    8 线程并行编译
#>

param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [ValidateSet("all", "linux", "uboot", "app", "linux+app")]
    [string]$Target = "all",

    [switch]$Clean,

    [string]$DockerImage = "swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0",

    [int]$Jobs = 4,

    [switch]$SkipLogin
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
# Step 3: 编译全部（Linux Kernel + U-Boot + 应用）
# ============================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Step 1/1: 编译 Linux Kernel + U-Boot + 应用" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# 编译脚本挂载到容器内执行，hwt/ 通过 -v 挂载到 /workspace/hwt
docker run --rm -v "${ProjectRoot}:/workspace" `
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
