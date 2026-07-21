<#
.SYNOPSIS
    带日志记录执行命令（固定文件名，每次覆盖，不堆积）
.DESCRIPTION
    1. 完整日志写入 logs/<名称>.log（UTF-8，每次运行覆盖，永远只有一份）
    2. 控制台只输出最后 20 行
    3. 另开终端可实时查看完整日志
.PARAMETER Name
    日志名称，如 docker-push、docker-build
.PARAMETER ScriptBlock
    要执行的命令块
.EXAMPLE
    .\run-with-log.ps1 docker-push { & ./docker-push.ps1 }

    构建镜像并推送，日志写入 logs/docker-push.log

.EXAMPLE
    .\run-with-log.ps1 docker-build { & ./docker-build.ps1 -Target image -SkipLogin }

    编译完整镜像，日志写入 logs/docker-build.log
.NOTES
    实时查看日志（另开一个 PowerShell 窗口）：
    Get-Content logs\docker-push.log -Wait -Tail 20
#>

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Name,

    [Parameter(Mandatory=$true, Position=1)]
    [scriptblock]$ScriptBlock
)

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$LogDir = Join-Path $ProjectRoot "logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$LogFile = Join-Path $LogDir "$Name.log"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  日志: $LogFile" -ForegroundColor Cyan
Write-Host "  (固定文件名，每次覆盖，不堆积)" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  实时查看请另开终端执行:" -ForegroundColor Yellow
Write-Host "  Get-Content '$LogFile' -Wait -Tail 20" -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Cyan

# *>&1 合并所有输出流（含 Write-Host/警告/错误），Out-File 流式写入实时落盘
# 每次运行覆盖旧文件，日志不堆积
& $ScriptBlock *>&1 | Out-File -FilePath $LogFile -Encoding utf8

# 控制台只显示最后 20 行
Write-Host ""
Write-Host "------ 最后 20 行 ------" -ForegroundColor DarkGray
Get-Content $LogFile -Tail 20

# 透传内部命令的退出码
if ($LASTEXITCODE) { exit $LASTEXITCODE }
