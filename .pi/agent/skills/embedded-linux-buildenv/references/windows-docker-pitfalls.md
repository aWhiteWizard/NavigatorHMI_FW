# Windows + Docker 构建环境踩坑

## 代理问题（最容易踩）

- Docker Desktop 默认使用 **Windows 系统代理**（注册表 `HKCU\...\Internet Settings` 的 ProxyEnable/ProxyServer）
- **Clash 未运行但系统代理开着** → daemon 连接 127.0.0.1:7890 被拒 → `docker login/push` 失败。
  临时关闭：`Set-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings' -Name ProxyEnable -Value 0`
- Clash fake-ip 模式时容器内 apt 可能 502（IP 显示 198.18.x.x）→ 重试即可
- **国内仓库（华为云 SWR、USTC 镜像）不需要代理**，直连更快
- 华为云 SWR 不支持新版 manifest：`docker build --provenance=false`

## Git Bash 路径转换

MSYS 会把 `/root/source/` 转成 `D:/Program Files/Git/root/source/` 导致容器内命令失败。
所有 `docker run` 加前缀：`MSYS_NO_PATHCONV=1 docker run ...`

## PowerShell 编码

- PS 5.1 把无 BOM 的 .ps1 当 GBK 解析 → 中文乱码。**写 PS 脚本必须存为 UTF-8 with BOM**：
  `[IO.File]::WriteAllText($p, $text, (New-Object Text.UTF8Encoding $true))`
- `Tee-Object` 默认写 UTF-16 → 改用 `*>&1 | Out-File -Encoding utf8`（能同时捕获 Write-Host）
- 给 AI 看输出时：`[Console]::OutputEncoding=[Text.Encoding]::UTF8`

## 挂载卷权限

- 从 Windows 侧删除/重建 `build/` 目录后，容器内 CMake 报
  `feature_tests.cxx Permission denied` → 在**容器内** `mkdir -p /workspace/build` 重建即可
- 挂载卷上 symlink 基本可用（kernel modules_install 验证过），但 IO 慢，大规模编译放容器 /tmp

## 长编译管理（1~2 小时级）

- 用 detached 容器：`docker run -d --name xxx-build ...`，不怕终端/会话中断
- 围观：`docker logs -f xxx-build`（用户自己开终端看，不消耗 AI token）
- 收尾：`docker logs xxx-build > logs/xxx.log` 归档后 `docker rm xxx-build`
- 短任务（<30min）可用日志包装脚本：固定文件名覆盖写入，日志不堆积

## Windows 睡眠

过夜编译前：电源设置"从不"睡眠（睡眠会暂停 Docker VM），暂停 Windows 更新自动重启。

## 双机同步清单

| 内容 | 同步方式 |
|------|---------|
| 源码 tarballs + 工具链 | 打进 Docker 镜像推 SWR，另一台 `docker pull` |
| hwt/ 覆盖层、构建脚本、defconfig | git 仓库 |
| build/ 编译缓存（含 Qt staging） | 不同步，各机编一次；或直接拷贝整个 build/qt5.12.9-arm |
| Docker 镜像 tag | 改 4 处：docker-push.ps1 默认值、docker-build.ps1 默认值、devcontainer.json、CI workflow |
