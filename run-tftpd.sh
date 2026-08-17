#!/bin/bash
# rk-tftpd 容器入口: 启动 tftpd 服务并保持容器存活 (v1.1 镜像已内置 tftpd)
service tftpd-hpa start 2>&1
sleep 1
service tftpd-hpa status 2>&1
tail -f /dev/null
