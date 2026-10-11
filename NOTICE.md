# 来源与许可证

- Linux 6.12.94 与 OpenWrt v25.12.5 generic 补丁：见 `sources.lock.json`。`kernel/patches` 将这套锁定补丁及本机 SoC 改动合并成一个可离线应用的补丁，不声称这些都是本项目原创。
- ZX279133 时钟、GPIO、pinctrl、MDIO、watchdog 和以太网参考 cnjn/linux-mainline-zte-zxslc-sr1010、cnjn/immortalwrt。SoC 基线文件及哈希见 `kernel/reference-provenance.json`；三网口驱动在参考驱动上修改，并保留 SPDX。
- Zlion-Y/e2631-mainline-zx279128s 提供了早期引导调试思路；E2631 镜像不能用于 SR7100。
- 无线使用 OpenWrt 锁定的 backports 与 mt76，另附板级 EEPROM 强制校验补丁。无线驱动、MediaTek 固件分别遵循各自上游许可证。
- 根目录 `LICENSE` 为 GPL-2.0 的许可证文本。已有文件的 SPDX、版权与双许可证声明优先；本项目新增的适配代码、构建和测试脚本按 GPL-2.0-only 发布。文档按 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) 发布。
- 不发布 NAND dump、原厂完整固件、PPU 微码、无线 EEPROM、设备密钥或订阅。运行所需 PPU 微码和本机校准数据须由设备所有者合法取得。

参考仓库：

- https://github.com/cnjn/linux-mainline-zte-zxslc-sr1010
- https://github.com/cnjn/immortalwrt
- https://github.com/Zlion-Y/e2631-mainline-zx279128s
- https://github.com/openwrt/openwrt
- https://github.com/openwrt/mt76
- https://github.com/vernesong/OpenClash
- https://github.com/MetaCubeX/mihomo
