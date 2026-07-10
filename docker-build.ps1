<#
.SYNOPSIS
    在 Docker 容器中交叉编译 NavigatorHMI_FW 项目 (i.MX6ULL ARM)
.DESCRIPTION
    使用 fw-builder-env Docker 镜像，在容器内完成 CMake 配置 + 编译，
    无需打开 VS Code Dev Containers。
.PARAMETER BuildType
    构建类型: Debug 或 Release (默认: Debug)
.PARAMETER NoRebuild
    跳过 CMake 重新配置，只重新编译 (相当于 make)
.PARAMETER Clean
    清理构建目录后重新编译
.PARAMETER DockerImage
    指定使用的 Docker 镜像 (默认: swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0)
.PARAMETER Jobs
    并行编译线程数 (默认: 4)
.EXAMPLE
    .\docker-build.ps1
    使用 Debug 模式编译

    .\docker-build.ps1 -BuildType Release
    使用 Release 模式编译

    .\docker-build.ps1 -Clean
    清理后重新编译

    .\docker-build.ps1 -NoRebuild
    只重新编译，跳过 cmake 配置阶段
#>

param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [switch]$NoRebuild,

    [switch]$Clean,

    [string]$DockerImage = "swr.cn-southwest-2.myhuaweicloud.com/image-linuxenv/fw-builder-env:v1.0",

    [int]$Jobs = 4
)

# ============================================================
# 脚本位置与路径
# ============================================================
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir    = Join-Path $ProjectRoot "build"

# ============================================================
# Step 0: 清理构建目录
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
# Step 1: CMake 配置（除非指定 --NoRebuild）
# ============================================================
if (-not $NoRebuild) {
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host "  Step 1/2: CMake 配置 (${BuildType})"      -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    # 清除可能存在的陈旧 CMakeCache.txt（来自 Windows 本机构建），避免路径冲突
    $CmakeConfigCmd = "rm -f /workspace/build/CMakeCache.txt && mkdir -p /workspace/build && cd /workspace/build && cmake .. -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/arm-linux-gnueabihf-toolchain.cmake -DCMAKE_BUILD_TYPE=${BuildType}"

    docker run --rm -v "${ProjectRoot}:/workspace" `
        -w /workspace `
        $DockerImage `
        /bin/bash -c "$CmakeConfigCmd"

    if ($LASTEXITCODE -ne 0) {
        Write-Host ">>> CMake 配置失败，退出" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host ">>> CMake 配置完成" -ForegroundColor Green
} else {
    Write-Host ">>> 跳过 CMake 配置 (--NoRebuild)" -ForegroundColor Yellow
}

# ============================================================
# Step 2: 编译
# ============================================================
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Step 2/2: 编译 (${Jobs} 线程)"            -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

$BuildCmd = "cd /workspace/build && make -j${Jobs}"

docker run --rm -v "${ProjectRoot}:/workspace" `
    -w /workspace `
    $DockerImage `
    /bin/bash -c $BuildCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host ">>> 编译失败" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "============================================" -ForegroundColor Green
Write-Host "  ✓ 编译成功!"                              -ForegroundColor Green
Write-Host "  输出文件: ${BuildDir}\bin\"               -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green

# 列出编译产物
if (Test-Path "${BuildDir}\bin") {
    Write-Host ">>> 编译产物:" -ForegroundColor White
    Get-ChildItem "${BuildDir}\bin" | ForEach-Object {
        Write-Host "    - $($_.Name)" -ForegroundColor White
    }
}
