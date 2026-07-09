@echo off
chcp 65001 >nul
REM ============================================================
REM NavigatorHMI_FW Docker 编译辅助脚本
REM 用法: docker-build [Debug|Release] [clean]
REM 示例:
REM   docker-build           - Debug 模式编译
REM   docker-build Release   - Release 模式编译
REM   docker-build Debug clean - 清理后 Debug 编译
REM ============================================================

powershell.exe -ExecutionPolicy Bypass -File "%~dp0docker-build.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo.
    echo 编译失败，请检查错误信息。
    pause
)
