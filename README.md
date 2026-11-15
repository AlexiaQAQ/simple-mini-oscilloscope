# simple-mini-oscilloscope

**基于 STM32F103C8T6 + ST7735S 的掌上示波器**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-blue.svg)
![IDE](https://img.shields.io/badge/IDE-Keil%20MDK%205.42-green.svg)
[![PCB](https://img.shields.io/badge/PCB-oshwhub-orange.svg)](https://oshwhub.com/alexia0/project_qvtauhlg)

单通道采集、自动纵向缩放、边沿触发，同时显示峰峰值 / 频率 / 周期 / 占空比，并带一路固定 1 kHz PWM 输出。
硬件开源：[立创开源硬件平台](https://oshwhub.com/alexia0/project_qvtauhlg)　·　代码许可：[MIT](LICENSE)

| 关键指标 | 值 |
|---|---|
| 采样率 | **100 Sa/s ~ 500 kSa/s**（TIM2 定时触发 ADC，非连续转换） |
| 时基 | **20 µs/div ~ 100 ms/div**，共 6 档 |
| 量程 | 1X：−1.3 ~ +4.8 V；10X：±13 V |
| 带宽 | 理论 250 kHz（Nyquist），实用约 50 kHz（10 点/周期） |
| 资源占用 | Flash 27 KB / 64 KB，RAM 3.5 KB / 20 KB |

## 快速开始

```bash
git clone https://github.com/AlexiaQAQ/simple-mini-oscilloscope.git
```

Keil 打开 `MDK-ARM/oscilloscope-project.uvprojx` → **F7** 编译 → **F8** 下载（ST-Link / J-Link / DAP-Link 均可）。
用 VSCode + EIDE 的话，新建工程后**导入 Keil 工程**即可，本地生成的 `.eide/` 不入库。

> ⚠️ **上电前把输入短接到 GND**，否则零点校准会把当前输入电平当成 0 V，之后所有读数带固定偏移。

---

## 1. 功能

- 单通道 ADC1_IN9（PB1），12 位，300 点/帧，绘制 154 点
- 6 档时基、1X / 10X 双量程、上升沿触发、**自动纵向缩放**（任意幅度撑满显示区）
- 测量：峰峰值 Vpp、频率（15 Hz ~ 200 kHz）、周期、占空比
- 暂停：旋钮按下冻结画面，LED 点亮 + 屏幕显示 `HOLD`
- 标定：上电 ADC 自校准 + 零点校准
- PWM 输出：TIM1_CH1（PA8），固定 1 kHz / 50%，常开

## 2. 硬件

### 2.1 平台与供电

| 项目 | 参数 |
|---|---|
| MCU | STM32F103C8T6（64 KB Flash / 20 KB RAM），8 MHz 晶振 → 72 MHz |
| ADC | 12 位 SAR，ADCCLK 12 MHz（PCLK2 ÷ 6），VDDA ≈ 3.2 V |
| 显示 | ST7735S，160 × 128，SPI1 @ 18 MHz |
| +5 V | IP5306（2.1 A 充电宝芯片）+ 18650 电池座，USB Type-C 输入 |
| −5 V | ICL7660 电荷泵（由 +5 V 反相），供运放负轨 |
| 3.3 V | XC6206P332 LDO，供 MCU / TFT / 比较器 |

> −5 V 靠电荷泵，上电建立要几十毫秒 —— 这是零点校准前要等 300 ms 的原因。

### 2.2 引脚分配

| 引脚 | 网络名 | 功能 | 备注 |
|---|---|---|---|
| PA5 / PA7 | `SCL-PA5` / `SDA-PA7` | SPI1_SCK / MOSI | 屏幕时钟、数据 |
| PB5 / PB6 / PB7 / PB8 | `RES` / `DC` / `CS` / `BLK` | GPIO 输出 | TFT 复位 / 命令 / 片选 / 背光 |
| PB1 | `ADC` | ADC1_IN9 | **信号输入**（前端 U6-B 输出） |
| PA8 | `PWM` | TIM1_CH1 | PWM 输出 |
| PB0 | `FREQ` | TIM3_CH3 | 输入捕获，接 **LM393 比较器输出** |
| PB4 / PB3 | `KEYA` / `KEYB` | EXTI 双边沿 | 编码器 A / B 相 |
| PA15 | `KEYD` | GPIO 输入（上拉） | 编码器按键 |
| PB13 / PB14 / PB15 | `KEY1` / `KEY2` / `KEY3` | EXTI 下降沿 | 三个功能键 |
| PC13 | — | GPIO 输出（开漏） | LED，低电平点亮 |
| PA13 / PA14 | `SWDIO` / `SWCLK` | SWD | 调试口 |

### 2.3 模拟前端

```
采集：BNC → SW3(DC/AC, 100nF) → SW4(1X/10X)
      → R14 820k / R6 82k / R8 100k 分压（C8 10pF、C7 90pF 补偿）
      → U6-A 缓冲 → R10 20k → U6-B 反相放大（R11 10k）→ PB1

测频：Uadc → U4-A(LM393) 1IN−，1IN+ ← R15/R18 分压 + R19 510k 正反馈 → PB0
```

- 输入阻抗 ≈ **1 MΩ**（820k + 82k + 100k），反相增益 **−R11/R10 = −0.5**，实测 `Uadc = 2.483 − 0.5065 × Vin`（1X）
- **比较器阈值固定** ≈ 2.19 V（对应输入 0.58 V @1X / 5.8 V @10X）：信号不跨越这个电平就没有边沿，频率测不到
- **AC/DC 与 1X/10X 都是面板机械开关**，固件读不到状态；1X/10X 的软件设置必须与开关一致，否则读数差 10 倍

### 2.4 关键器件

| 位号 | 型号 | 作用 |
|---|---|---|
| U2 | STM32F103C8T6 | 主控 |
| U3 | ST7735（1.8" TFT） | 显示 |
| U6 | TL072（±5 V 双运放） | 输入缓冲 + 反相放大 |
| U4 | LM393（双比较器） | 测频整形 |
| U7 / U1 / U8 | IP5306 / ICL7660 / XC6206 | 电池管理 / −5 V / 3.3 V |
| SW2 | EC11L1525 | 旋转编码器 |
| SW3 / SW4 | SK-12E12 | DC-AC / 1X-10X 拨动开关 |

## 3. 操作

| 操作 | 功能 |
|---|---|
| **KEY1**（PB13） | 选中「量程」→ 旋钮切 1X / 10X |
| **KEY2**（PB14） | 选中「时基」→ 旋钮切 6 档 T/div |
| **KEY3**（PB15） | 选中「触发电平」→ 旋钮调节 |
| **旋钮旋转** | 调整当前选中项 |
| **旋钮按下** | 暂停 / 继续 |

选中的项显示为**黄底黑字**。按键实现：KEY1~3 走 EXTI 中断 + 主循环延时确认（非阻塞消抖）；编码器按键走软件边沿检测（按下即响应）；A/B 相都走中断，用**四边沿状态机**解码。

**上电流程**：输入接地 → 上电，等约 0.5 s（ADC 自校准 + 屏幕初始化 + 零点校准）→ 接入信号。

## 4. 界面

```
┌──────────────────────────────────────────────┐
│ AC/DC                                 1X/10X │ ← 固定图例
├──────────────────────────────────────────────┤
│              WAVES 154 × 56                  |◀ 触发电平三角
├──────────────────────────────────────────────┤
│ ─┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬─   │ 横轴（每格 10 px）
│ VPP   1.02V    FRQ    47KHz                  │ 峰峰值 / 频率
│ PER  21.3us    DTY      50%                  │ 周期 / 占空比
│ 1X     210us        T1.00V                   │ 量程 / 时基 / 触发
└──────────────────────────────────────────────┘
```

ADC 饱和时 Vpp 变**红色**（读数不可信，换 10X 档）。右侧红三角是触发电平的纵向位置。

### 时基档位

`T/div = 10 × 触发周期`，采样率由 TIM2 触发周期决定，与 ADC 转换时间无关。

| 档 | ADC 采样时间 | 触发周期 | T/div | 采样率 | 每帧（300 点） |
|---|---|---|---|---|---|
| 1 | 1.5 周期 | 2 µs | 20 µs | 500 kSa/s | 0.6 ms |
| 2 | 28.5 | 5 µs | 50 µs | 200 kSa/s | 1.5 ms |
| 3 | 55.5 | 20 µs | 200 µs | 50 kSa/s | 6 ms |
| 4 | 71.5 | 200 µs | 2 ms | 5 kSa/s | 60 ms |
| 5 | 239.5 | 2 ms | 20 ms | 500 Sa/s | 0.6 s |
| 6 | 239.5 | 10 ms | 100 ms | 100 Sa/s | 3 s |

## 5. 软件结构

```
Core/                  ← CubeMX 生成，只做外设初始化
  Src/main.c           初始化 + 主循环
  Src/tim.c            TIM1(PWM) / TIM2(ADC 触发) / TIM3(测频捕获)
  Src/gpio.c           GPIO + 按键/编码器 EXTI
App/                   ← 业务逻辑
  app_scope.c          电压换算、测量、触发、绘制、零点校准、暂停
  app_key.c            按键扫描、编码器解码、时基切换、LED
  app_ui.c             界面布局与刷新（变化检测）
  app_freq.c           测频（输入捕获 + 16 周期平均 + 超时清零）
Hardware/              ← 驱动层
  tft.c                绘图原语   tft_init.c  ST7735S 初始化（含 SWRESET）
  font.h               点阵字库
```

数据流：`TIM2 触发 ADC ──DMA──> adc_value[300] ──> 换算/测量 ──> 绘制 + 读数`，
测频独立：`LM393 ──> TIM3 捕获中断 ──> 16 周期平均 ──> 频率/周期`。

## 6. 关键参数与标定

```
ADC码值 ──> 引脚电压 ──> 输入电压
x[V] = raw × ADC_VREF / 4096
Vin  = (zero_pin − x) × ADC_FRONTEND_GAIN × (10X ? 10 : 1)
```

| 常量 | 位置 | 本板值 | 含义 |
|---|---|---|---|
| `ADC_VREF` | `app_scope.c` | 3.334 V | 实测 VDDA |
| `ADC_FRONTEND_GAIN` | `app_scope.c` | 1.97 | 前端衰减比倒数（零点 2.483 V、斜率 0.5065） |
| `ZERO_CAL_DEFAULT` | `app_scope.c` | 2.37 V | 零点校准失败时的兜底值 |

**重新标定**：零点不用改代码（输入接地重新上电即可自动测量）；增益接入已知直流后按比例修正
`GAIN_new = GAIN_old × 真实值 / 显示值`。换板、换 VDDA 稳压、改前端电阻后必须重标。

## 7. 编译与烧录

| 组件 | 版本 |
|---|---|
| Keil MDK-ARM | 5.42（ARMCC V5.06 / AC5） |
| STM32F1xx_DFP | 1.1.0 |
| STM32CubeMX | 6.x（仅用于改外设，重新生成 `Core/`） |
| STM32Cube FW_F1 | V1.8.x（HAL 已随仓库提供） |

**用 CubeMX 重新生成后请核对**：PA15 为上拉输入（不是 EXTI）、PB13/14/15 为 EXTI 下降沿、PB3/PB4 为 EXTI 双边沿、ADC1 为外部触发 T2_CC2 + 连续转换关闭 + DMA 循环。
另外 CubeMX 生成的 Keil 工程**不含 `App/` 和 `Hardware/` 分组**，需要手动加回（含 `../App/inc`、`../Hardware/inc` 路径）。

## 8. 已知限制与常见问题

| 现象 / 限制 | 原因 | 处理 |
|---|---|---|
| **上电白屏** | 面板首次上电忽略初始化命令 | `TFT_Init()` 已内置 SWRESET；仍复现则把 `main.c` 的 `TFT_INIT_TWICE` 设为 1 |
| **波形整体平移、Vpp 却准** | 零点校准采到了非 0 V 输入 | 上电前输入接地，重新上电 |
| **读数差 10 倍** | 1X/10X 开关与软件设置不一致 | KEY1 选中量程后切到一致 |
| **方波底部显示负值** | AC 耦合去掉了直流分量 | SW3 切到 DC 耦合 |
| **不接信号 Vpp 显示 0.02 V** | ADC 噪声底 | 低于 30 mV 一律显示 0，属正常 |
| **频率显示 0 / 突然掉到 0** | 信号不跨比较器阈值，或 1.5 s 无捕获 | 加大信号幅度，或改 R17 降低阈值 |
| **频率长时间不刷新** | 16 周期平均 | 15 Hz 时约 1 秒更新一次，正常 |
| **旋钮转一格跳两下 / 两格才走一步** | 边沿数与 `ENC_EDGES_PER_STEP` 不匹配 | 改 `app_key.c` 里的宏（1/2/3/4） |
| **旋钮方向反了** | A/B 相接反 | 对调 `KEYAPRESS` / `KEYBPRESS` |
| **慢档几秒才更新一次** | 100 ms/div 时一帧要 3 s | 正常现象，相当于"卷动模式" |
| **编译报 `Could not open via file ….__i`** | 输出目录混了旧构建产物 | 清空 `MDK-ARM/oscilloscope-project/` 后重建 |
| 单缓冲 | 无撕裂，但无法连续流式采集 | 可改双缓冲 |
| 刷屏慢 | 逐字节写 SPI，每 2 字节翻转 CS | 可改块传输 |
| 15 Hz 以下读数不可信 | 16 位计数器 + 只补偿一次回绕 | 硬件限制 |
| 无垂直档位 | 只有自动缩放，幅度变化时波形会"呼吸" | 可加手动 V/div |
| 无输入保护 | Uadc 可能被运放拉到负压 | 建议 ADC 引脚加钳位 |
| SPI 18 MHz 超手册 | ST7735S 上限 15 MHz | 出现花屏时把 SPI1 分频改成 8（9 MHz） |

## 9. 后续可做

1. **双缓冲采集**：半传输 + 传输完成中断交替，消除偶发毛刺；
2. **块传输刷屏**：CS 常低 + 连续写整行，帧率可提升数倍；
3. **垂直档位 / 单次触发**：手动 V/div、捕获瞬态事件。

## 10. 许可与致谢

| 对象 | 许可 |
|---|---|
| 本仓库代码（`App/`、`Hardware/`、`Core/` 中自行编写部分） | **MIT**，见 [LICENSE](LICENSE) |
| 硬件设计（原理图 / PCB / BOM） | 见[开源硬件页面](https://oshwhub.com/alexia0/project_qvtauhlg)声明 |
| ST HAL / CMSIS（`Drivers/`） | BSD 3-Clause，版权归 STMicroelectronics |
| TFT 点阵字库与初始化序列 | 由常见开源 TFT 驱动整理修改 |

感谢立创开源硬件平台（原理图 / PCB / 器件选型）、STMicroelectronics（HAL 库与参考手册），以及提供 TFT 驱动与字库的开源社区作者。

欢迎提 Issue / PR。提交前请确认 Keil 编译 **0 error 0 warning**，且不提交构建产物（`.gitignore` 已排除）。
