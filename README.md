# ZTE SR7100 OpenWrt 适配

中兴 SR7100（ZX279133 + MT7992）的社区适配源码，基于 **OpenWrt 25.12.5 / Linux 6.12.94**。开发记录见[移植过程](docs/porting-story.md)。

实机已经运行独立启动的 OpenWrt：WAN、LAN1、LAN2、双频 Wi-Fi、LuCI、OpenClash，以及 NAND 上的持久配置。当前布局保留两份原厂 U-Boot，把 B 系统载荷空间扩入 Plugin；Mihomo 在 Plugin 中只保存当前压缩核心，rootfs 留有原始核心兜底。

**这是已在一台设备验证过的开发源码，不是 OpenWrt 官方支持机型，也不是通用 sysupgrade 固件。** 本仓库可以重建内核、网口和无线模块、板级 DTB，并提供启动及存储集成代码。没有附带个人配置的成品镜像，也没有将设备专用刷写流程伪装成一键安装器。

## 硬件与实测范围

| 项目 | 结果 |
| --- | --- |
| CPU / RAM | ZX279133，双核 Cortex-A53，512 MiB DDR3 |
| NAND | 本机为 Winbond `ef-ae-21`，128 MiB，128 KiB 擦除块、2 KiB 数据页 |
| 无线 | MT7992，PCIe Gen3 x2；mt76；本机 EEPROM |
| 端口 | WAN = SMAC0 / PHY 0x0a；LAN1 = SMAC1 / PHY 0x0b；LAN2 = SMAC2 / PHY 0x0c |
| 网络 | 三口共享 DMA；LAN 桥、WAN DHCP/NAT、隔离、双频关联与重连已实测 |
| 启动 | 原厂 U-Boot 启动 A 系统；普通重启和真实冷启动已通过 |
| 存储 | Plugin 原始容量 73.25 MiB；UBIFS `df` 总量约 60.9 MiB |
| 核心更新 | OpenClash 更新后持久保存；单核心策略普通重启已验证 |

未完成长期稳定性、极限吞吐、完整 IPv6、真实 PPPoE 接入和官方 sysupgrade 集成。不要将表中的功能验收理解成这些项目也已通过。PPPoE 内核组件已启用，但没有 ISP 实测。源码重建与实机验收的具体范围见 [VALIDATION.md](docs/VALIDATION.md)。

## 目录

- `kernel/`：锁定内核配置、OpenWrt generic + SR7100 合并源码补丁及逐文件校验。
- `drivers/ethernet/`：参考驱动及 SR7100 三口共享 DMA、PHY 和收发归属改动。
- `dts/`：RAM 全只读 NAND、安装后仅 Plugin 可写的两份设备树。
- `wireless/`：强制要求本机 EEPROM 的 mt76 补丁。
- `userspace/`：initramfs 启动、受限 UBI 操作、单核心持久化、只读核心兜底。
- `esp32-uart-bridge/`：临时 USB 串口桥和默认只接收的监视程序。
- `tools/`、`tests/`：可复现构建、离线布局检查和故障注入测试。

## 构建

依赖与构建入口见 [BUILD.md](docs/BUILD.md)。所有公开构建和测试脚本只操作本地文件，不连接设备。

```sh
docker build -t sr7100-builder .
docker run --rm -v "$PWD:/src" sr7100-builder sh tools/build-kernel.sh
docker run --rm -v "$PWD:/src" sr7100-builder sh tools/build-wireless.sh
docker run --rm -v "$PWD:/src" sr7100-builder sh tools/test.sh
```

DTB 必须使用自己的 EEPROM 和 MAC，输出包含私人校准信息，不要上传：

```sh
docker run --rm -v "$PWD:/src" sr7100-builder \
  python3 tools/compile-dts.py --mode ram \
  --eeprom private/mt7992-eeprom.bin --mac 02:00:00:00:00:01
```

示例 MAC 仅用于说明参数。实际网络中使用自己设备的有效、唯一地址。EEPROM、原厂 PPU 微码、备份和任何生成的镜像均放在忽略目录，不能从另一台路由器复制校准数据。

## 安装与恢复

先读[分区与恢复](docs/RECOVERY.md)。初始开发必须有独立于待测路由器网络的管理链路、串口和经验证的 U-Boot/TFTP 恢复路径。读取备份成功不等于写回恢复成功。

**B 系统已经被回收，B U-Boot 保留。A 内核挂死并不等于设备会自动切换到另一个完整 Linux。** 本机保留的 B 救援定位/版本头用于早期引导寻找 B U-Boot；A 头损坏的回退分支以实际早期代码仿真验证，未在真机故意破坏 A 头。

通用 `sysupgrade`、`firstboot`、`ubiformat` 不应直接套用这份布局。原实验的地址、版本头、备份哈希和恢复判定均与单台样本绑定，不能去掉校验后用于其他机器。

## 来源

这个移植建立在 Linux、OpenWrt、cnjn 的 ZX279133/SR1010 适配和 mt76 等工作上；参见 [NOTICE.md](NOTICE.md) 与 `sources.lock.json`。保留每个文件的原有许可证，原厂二进制和本机私密资料不在发布范围。
