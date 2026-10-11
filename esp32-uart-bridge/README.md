# ESP32-C3 临时 USB 串口桥

用于手边没有 USB-UART 模块时观察 SR7100。固件把 USB Serial/JTAG 与 UART1 双向转发，115200 8N1，不修改换行，也不自动向路由器发送命令。

| ESP32-C3 | 路由器 |
| --- | --- |
| GND | GND |
| GPIO5 / RX | 路由器 TX |
| GPIO4 / TX | 路由器 RX |

路由器 UART 电平须确认是 3.3 V。本次实机测得约 3.3 V；其他设备需单独测量。ESP32 用 USB 供电，路由器单独供电，**不接路由器 VCC**。观察输出只需 GND 与 GPIO5；确认 TX/RX 后再连接 GPIO4。

```sh
pio run
pio run --target upload --upload-port /dev/ttyACM0
uvx --with pyserial python monitor.py --port /dev/ttyACM0
```

串口程序默认只接收，键盘输入不会发给路由器。需要交互时加 `--interactive`，Ctrl-] 退出。加 `--enter-boot` 会在看到厂商 `entering csp boot.` 提示时只发送一个 ASCII `1`；不会自动输入密码或刷写命令。需要交互进入菜单时可以同时加两项。

ESP32 自身启动日志和 `C3 bridge READY` 只说明桥接程序启动，不代表已经接对路由器 TX。内部 UART 自检使用内部回环，不能证明外部焊点和接线正确。
