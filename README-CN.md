# Mini Analog Electronics（迷你模拟电子学）

一套**从零开始、零依赖的 C 语言实现**，涵盖大学级模拟电子电路与器件。每个模块对应 MIT 及其他顶尖大学的一门或多门课程，将教科书中的电路理论转化为可运行的 C 模型，覆盖二极管物理、晶体管放大器、运放电路、有源滤波器、反馈系统、振荡器以及模拟集成电路设计。

## 子模块

| 子模块 | 主题 | 参考课程 |
|-----------|--------|-------------|
| [mini-active-filter](mini-active-filter/) | Sallen-Key、MFB、开关电容、Gm-C、N-path 滤波器、级联设计 | MIT 6.002, Stanford EE101B, Berkeley EE105 |
| [mini-analog-ic-design](mini-analog-ic-design/) | MOS/BJT 器件模型、电流镜、差分对、带隙基准、噪声 | Berkeley EE140/EE240, Stanford EE214, MIT 6.775 |
| [mini-bjt-amplifier](mini-bjt-amplifier/) | BJT 直流偏置、小信号模型、频率响应、共源共栅、达林顿、多级放大 | MIT 6.002, Berkeley EE105, Stanford EE114 |
| [mini-diode-rectifier](mini-diode-rectifier/) | PN 结物理、肖克利方程、半波/全波整流器、滤波电容、缓冲保护 | MIT 6.002, Berkeley EE105 |
| [mini-feedback-amplifier](mini-feedback-amplifier/) | 四种反馈拓扑（S-S / Sh-Sh / S-Sh / Sh-S）、稳定性分析、频率补偿 | MIT 6.002, Berkeley EE105, Stanford EE114 |
| [mini-fet-amplifier](mini-fet-amplifier/) | FET 直流偏置、小信号模型、交流频率分析、米勒定理、噪声 | MIT 6.002, Berkeley EE105, Stanford EE114 |
| [mini-opamp-application](mini-opamp-application/) | 理想/非理想运放模型、基本电路、有源滤波器、非线性电路、稳定性 | MIT 6.002, MIT 6.775, Stanford EE214 |
| [mini-oscillator-circuit](mini-oscillator-circuit/) | 巴克豪森判据、LC/RC/张弛/晶体振荡器、相位噪声、锁相环 | MIT 6.002, Berkeley EE105, Stanford EE114 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **理论到代码的映射** — 每个模块包含 `docs/` 目录，内含课程对齐说明及教材参考（Sedra & Smith、Razavi、Gray & Meyer）
- **实用演示程序** — 滤波器设计器、放大器仿真器、整流器计算器、振荡器分析器等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-active-filter
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```text
mini-analog-electronics/
├── mini-active-filter/          # 有源滤波器（Sallen-Key、MFB、SCF、Gm-C）
├── mini-analog-ic-design/       # 模拟集成电路设计（MOS/BJT、电流镜、带隙基准）
├── mini-bjt-amplifier/          # BJT 放大器（偏置、小信号、共源共栅）
├── mini-diode-rectifier/        # 二极管物理与整流电路
├── mini-feedback-amplifier/     # 反馈放大器拓扑与稳定性
├── mini-fet-amplifier/          # FET 放大器（偏置、交流分析、噪声）
├── mini-opamp-application/      # 运算放大器应用
└── mini-oscillator-circuit/     # 振荡器电路（LC、RC、晶体、PLL）
```

## 许可证

MIT
