# 构建与集成

## 可以直接重建的部分

`tools/build-kernel.sh` 从校验过的 Linux 6.12.94 压缩包开始，应用完整的锁定源码差异，逐个核对改动文件，再构建内核和三网口模块。合并补丁包含 OpenWrt generic 改动及 SoC 支持；不要再额外应用一遍 OpenWrt generic 补丁。`kernel/source-delta.json` 记录基线及最终文件哈希。

`tools/build-wireless.sh` 使用 `sources.lock.json` 中的 backports、mt76，拉取固定 OpenWrt commit 的包补丁，然后编译到同一内核。输出 `compat`、`cfg80211`、`mac80211`、`mt76`、`mt76-connac-lib`、`mt7996e`。不能把官方 ARM64 包中的 kmod 与此处内核混用。

构建可以运行在 arm64 或 x86_64 Linux 上，交叉工具链默认为 `aarch64-linux-gnu-`。提供的 Dockerfile 安装工具链；构建脚本不会访问路由器。

## 设备树与专用输入

`dts/sr7100-ram.dts` 来自实测设备树的预处理文本，保留标签和寄存器说明。`sr7100-installed.dts` 仅变更 SFC compatible 以及 Plugin 的可写属性。历史 compatible 中的 `f35sqa` 是最初样本命名，不代表实机 NAND 型号。

`tools/compile-dts.py` 要求 7680 字节、MT7992 ID 的本机 EEPROM。长度和 ID 只能排除明显错误，不能证明校准参数属于当前 PCB。读出来源必须由使用者确认。私有 DTB 中包含 EEPROM，不提供公开预编译 DTB。

以太网还需要原厂用户空间使用的 `mcode_mtk7992.bin`，在系统内放到 `/lib/firmware/zte/sr7100/mcode_mtk7992.bin`。驱动通过 `request_firmware()` 装载。仓库不捆绑这个原厂微码。无线 WM/WA/DSP/ROM patch 从锁定 mt76 源码的 `firmware/mt7996/` 获取，并遵守相应固件许可证。

## 用户空间

原实机使用 OpenWrt 25.12.5 armsr/armv8 rootfs，加 LuCI、firewall4、dnsmasq-full、wpad-mbedtls、wifi-scripts、ucode-mod-digest、OpenClash 及其依赖。这里公开的是硬件相关集成代码，**没有把当时的私有 rootfs 和所有软件包缓存作为公开固件发布**。

安装时需要将自己准备的 rootfs 与匹配模块组装，并准备自己的网络、管理员密码、无线密码、地区码和代理配置。三口设备名为 `wan`、`lan1`、`lan2`；后两者加入 `br-lan`，WAN 单独配置 DHCP 或自己使用的接入方式。MT7992 是一个 `phy0` 下的两个 radio index，2.4 GHz/5 GHz 分别为 0/1。本机使用 CN；地区码应与实际所在地一致。

根文件系统与启动包分离：

1. `rootfs.squashfs` 位于只读 A rootfs 分区。
2. 小型 initramfs 包含 BusyBox、musl 依赖、`userspace/ubi-control.c` 编译出的 `/bin/sr7100-plugin-ubi` 和 `init-installed`。
3. `userspace/boot.its` 组装内核、小 initramfs 与板级 DTB。
4. RAM 版本使用 `init-ram`，在 initramfs 中另附 `rootfs.squashfs`，所有 NAND 阵列写操作拒绝。

FIT 模板不是原厂可刷格式。正式 NAND 启动还涉及原厂定位标记、加密载荷、版本头 CRC、槽位和内存窗口校验，见 [RECOVERY.md](RECOVERY.md)。本仓库不会输出可以盲刷的通用升级包。

## Mihomo 持久化

在已装有匹配 OpenClash 的**离线 rootfs**上运行：

```sh
python3 tools/install-core-hooks.py build/rootfs \
  --fallback-core private/mihomo-arm64
```

适配基于 OpenClash 0.47.156 的脚本结构。安装器核对锚点，不匹配时停止，不能静默假设任意新版插件都兼容。

将 `small_flash_memory` 设为 1，核心在 RAM 执行；只读回退核心放在 `/usr/share/sr7100/openclash/core/clash_meta`。`sr7100-core-store` 把当前压缩核心保存在 `/overlay/sr7100-core/`，要求 UBIFS 和 `sr7100-plugin-expanded-v2` 布局标记。提交前运行新核心校验当前配置，提交后删除旧归档；临时更新期间仍需容纳新旧两份归档，空间不足会拒绝更新并保留当前版本。

`openclash-runtime` 在服务启动时恢复 Plugin 核心，不可用则补入只读核心。GeoSite 和 Country.mmdb 可由自己的镜像提供；`Country.mmdb.gz` 种子路径为 `/etc/sr7100/openclash/`。完整 OpenClash 包升级可能替换定制脚本，不在本次兼容性保证内。

测试命令：

```sh
sh tools/test.sh
```

测试包括 NAND 每页写入边界、端口来源与共享生命周期、核心配置拒绝/空间不足/损坏归档/八个提交边界强制终止。这些是软件故障注入，不能等同于真实 NAND 在任意时刻断电都安全。
