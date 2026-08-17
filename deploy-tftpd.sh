#!/bin/bash
# RK3562 烧写环境 TFTP 服务器部署 (B-1)
# 用法: docker run -d -p 69:69/udp -v <firmware目录>:/srv/tftp image bash /workspace/deploy-tftpd.sh
LOG=/workspace/tftpd-deploy.log
exec > ${LOG} 2>&1

echo "=== 安装 tftpd ==="
DEBIAN_FRONTEND=noninteractive apt-get update -qq >/dev/null 2>&1
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq tftpd-hpa tftp-hpa >/dev/null 2>&1
echo "apt exit=$?"

echo "=== 配置 ==="
mkdir -p /srv/tftp
chmod 777 /srv/tftp
cat > /etc/default/tftpd-hpa <<'EOF'
TFTP_USERNAME="tftp"
TFTP_DIRECTORY="/srv/tftp/"
TFTP_ADDRESS="0.0.0.0:69"
TFTP_OPTIONS="-l -c -s"
EOF

echo "=== 启动 ==="
service tftpd-hpa start 2>&1
sleep 2
service tftpd-hpa status 2>&1

echo "=== 自测: 本机 tftp 拉文件 ==="
cd /tmp
printf 'get boot.img\nquit\n' | timeout 15 tftp 127.0.0.1 2>&1
ls -la /tmp/boot.img 2>&1
echo "=== 完成 ==="
# 保持容器存活
tail -f /dev/null
