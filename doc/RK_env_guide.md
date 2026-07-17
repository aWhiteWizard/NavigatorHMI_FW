1. 编译 Linux 固件
为了方便用户的使用与开发，官方提供了 Linux 开发的整套 SDK，本章详细的说明 SDK 的具体用法。

*SDK路径：*
1.1. 准备工作
1.1.1. 搭建 SDK 编译环境
以下文件请务必确认安装！

这里使用Ubuntu18.04进行测试(推荐使用ubuntu18.04系统进行开发，或者使用 docker 部署 Ubuntu18.04 容器，否则无法安装好环境包)：

sudo apt-get update

sudo apt-get install repo git-core gitk git-gui gcc-arm-linux-gnueabihf u-boot-tools device-tree-compiler \
gcc-aarch64-linux-gnu mtools parted libudev-dev libusb-1.0-0-dev python-linaro-image-tools \
linaro-image-tools gcc-arm-linux-gnueabihf libssl-dev liblz4-tool genext2fs lib32stdc++6 \
gcc-aarch64-linux-gnu g+conf autotools-dev libsigsegv2 m4 intltool libdrm-dev curl sed make \
binutils build-essential gcc g++ bash patch gzip bzip2 perl tar cpio python unzip rsync file bc wget \
libncurses5 libqt4-dev libglib2.0-dev libgtk2.0-dev libglade2-dev cvs git mercurial rsync openssh-client \
subversion asciidoc w3m dblatex graphviz python-matplotlib libssl-dev texinfo fakeroot \
libparse-yapp-perl default-jre patchutils swig chrpath diffstat gawk time expect-dev
注意： Ubuntu17.04 或者更高的系统还需要如下依赖包：

sudo apt-get install lib32gcc-7-dev g++-7 libstdc++-7-dev
1.1.2. 下载 Firefly_Linux_SDK 分卷压缩包
由于 Firefly_Linux_SDK 源码包比较大，部分用户电脑不支持4G以上文件或单个文件网络传输较慢, 所以我们采用分卷压缩的方法来打包SDK。用户可以通过如下方式获取 Firefly_Linux_SDK源码包：Firefly_Linux_SDK源码包

1.1.3. 解压 Firefly_Linux_SDK 分卷压缩包
第一次使用SDK需执行3个步骤，如果是后续想更新SDK，只需执行第3步进行网络更新即可

1. 解压SDK

chmod +x ./sdk_tools.sh

创建一个目录以存放SDK：比如我现在这个是3588的SDK，我想解压到上一层文件夹，避免污染当前目录

mkdir ../firefly_rk3588_SDK
./sdk_tools.sh --unpack -C ../firefly_rk3588_SDK

2. 还原工作目录

选择刚才解压后的目录

./sdk_tools.sh --sync -C ../firefly_rk3588_SDK

可以使用上面脚本执行或者手动执行命令，选择其中一种即可

# 进入刚刚解压后的目录，比如我这里是../firefly_rk3588_SDK
cd ../firefly_rk3588_SDK
.repo/repo/repo sync -l
.repo/repo/repo start firefly --all

3. 更新SDK

前面2个步骤只在第一次解压SDK时执行，后续更新SDK只需进入SDK目录执行第3步骤，进行网络更新

.repo/repo/repo sync -c --no-tags
1.1.4. 更新 Firefly_Linux_SDK
后续可以使用以下命令更新 SDK

.repo/repo/repo sync -c --no-tags