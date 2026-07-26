# 芯片手册索引

> 文档位置: `D:\workspace\image_sources\【正点原子】I.MX6U嵌入式Linux驱动开发指南V1.5\chip data sheet\`

按芯片分类的表格，包含芯片型号、功能、PDF 文件名及关键参数速记。

| 芯片型号 | 功能 | PDF 文件名 | 关键参数速记 |
|---|---|---|---|
| GT9147 | 电容触摸 IC | `GT9147数据手册.pdf` + `GT9147编程指南.pdf` | I2C地址 0x14, 状态寄存器 0x814E, 坐标数据 0x8157起, 每点8字节 (TrackID+X2+Y2+Size2+Reserved1), 中断清除写0到0x814E, 上电需写 Config_Fresh (0x8100=0x01) |
| LAN8720A | 以太网 PHY | `LAN_8720A-CP_datasheet.pdf` | RMII, PHY地址可由PHYAD0 strap, 复位脚接 SNVS_TAMPER7/8 |
| WM8960 | 音频 codec | `WM8960_v4.2.pdf` | I2C 地址 0x1a, **本板未贴** |
| OV5640 | 摄像头 sensor | `OV5640_CSP3_DS_2.01_Ruisipusheng.pdf` | I2C 地址 0x3c, **本板未贴** |
| FT5426 | 电容触摸 IC | `FT5426(电容触摸屏IC)/` 目录 | I2C地址 0x38, **本板未用** |
| AP3216C | 环境光/接近传感器 | `AP3216C.pdf` | I2C1 地址 0x1e |
| ICM-20608 | 六轴陀螺仪/加速度计 | `ICM-20608-G Datasheet.pdf` | SPI/I2C, **本板未贴** |
| SiI9022A | HDMI 发送器 | `SiI9022A9024A-DS-1076-C01.PDF` | I2C2, **本板未贴** |
| KLM8G1GETF | eMMC 8GB | `KLM8G1GETF-B041-Samsung.pdf` | eMMC 5.0, mmcblk1 |
| NT5CC256M16 | DDR3 512MB | `NT5CC256M16EP-EK.pdf` | 512MB |

## 使用提示

- 下次用到某芯片，让文档检索员直接 grep 对应的 `.txt` 文件（如果有转好的）
- 或让审图员读 PDF 里的寄存器表
