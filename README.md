<!--
 * @Author: aWhiteWizard www.123518341@qq.com
 * @Date: 2026-07-09 23:19:00
 * @LastEditors: aWhiteWizard www.123518341@qq.com
 * @LastEditTime: 2026-07-09 23:53:27
 * @FilePath: \NavigatorHMI_FW\README.md
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
-->

# NavigatorHMI_FW
Embedded device code for NavigatorHMI

# complie method

## Debug 编译（默认）
.\docker-build.ps1

## Release 编译
.\docker-build.ps1 -BuildType Release

## 清理后重新编译
.\docker-build.ps1 -Clean

## 跳过 CMake 配置，只重新 make
.\docker-build.ps1 -NoRebuild

## 指定并行线程数
.\docker-build.ps1 -Jobs 8
