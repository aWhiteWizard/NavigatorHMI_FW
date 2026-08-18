#!/bin/bash
# B-2 Do: 备份官方 rootfs 分区 (bundle 升级用, sparse)
LOG=/workspace/diag-ddrootfs.log
exec > ${LOG} 2>&1

echo "=== dd 备份 rootfs (sparse) ==="
time dd if=/dev/disk/by-partlabel/rootfs of=/var/lib/rknetupdate/bundle-v1/rootfs.img bs=4M conv=sparse,fsync 2>&1
echo "dd exit=$?"

echo "=== 备份文件 ==="
ls -la /var/lib/rknetupdate/bundle-v1/rootfs.img 2>&1
du -sh /var/lib/rknetupdate/bundle-v1/rootfs.img 2>&1
echo "=== 完成 ==="
