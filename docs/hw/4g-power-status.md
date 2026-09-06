# 4G 模块供电状态报告（P0 阻塞）

**结论**：p4c5 板子硬件设计**必须电池供电**，USB-C 不能直接给 ML307C 供电。

## 1. 物理事实（DSH 调研）

| 测试点 | 期望 | 实测 | 结论 |
|---|---|---|---|
| VBUS (USB 5V) | 5000 mV | **0 mV** | AXP2101 没检测到 USB 5V |
| VBAT (电池) | 0-4200 mV | **0 mV** | 无电池 |
| ALDO4 (ML307C 电源) | 2900 mV | reg=0x18, **0V**（无电源输入）| 输出空载 |
| PMU_STATUS_1 (reg 0x00) | 0x01 或 0x04 | **0x20** | AXP2101 内部电源路径空 |

## 2. 硬件设计真相

原理图调研后发现 p4c5 板的电源链路：
1. **J5 (侧面 USB-C)** — VBUS 进 `BOOST_5V` 模块（升压 5V 给 USB-A 输出口）
2. **J5 VBUS 未直接进 AXP2101 VBUS_IN**
3. **VBUS_IN 是 P4C5 模块内部引脚**（板上走线）
4. **J2 (电池接口 MX1.25-2P)** — 才是 p4c5 板子的主电源输入

## 3. 用户实测

卖家提示"4G 只能电池供电" — 卖家说得对！

- 接 5V/2A 充电器到 J5 → ESP32-P4 模块通过模块内置电源管理自己跑（屏幕亮 + heartbeat）
- 但 AXP2101 检测不到外部电源 → ALDO4 不输出 → ML307C 没电

## 4. 软件状态

| 任务 | 状态 | 说明 |
|---|---|---|
| T19 PWRKEY 时序 | ✅ 已加 | 拉低 200ms 触发 |
| T15 ADC 自测 | ✅ 已加 | 启用了 ADC enable + PMU_STATUS 读回 |
| ML307C 拨号 | ❌ 阻塞 | 无 VBAT 电源 |

## 5. 解锁方法

任一即可：
1. **接电池**到 J2（MX1.25-2P 接口，锂电池 3.7V）
2. 改板硬件：J5 VBUS 串 0R 跳线到 AXP2101 VBUS_IN
3. 用外部稳压 3.7V 直供 J2 电池接口（模拟电池）

## 6. 决策

用户选择 **跳过 4G 模块验证**，专注其余 MVP（屏幕/语音/UI）。

4G 模块在最终产品测试阶段**必须用电池**才能验证拨号。

## 7. 关键代码

### `p4c5_pmic.cc::p4c5_pmic_get_vbus_mv()`
```cpp
uint16_t p4c5_pmic_get_vbus_mv(void) {
    if (!s_initialized) return 0;
    uint8_t adc_en = pmic_i2c_read_reg(AXP2101_REG_ADC_ENABLE);
    pmic_i2c_write_reg(AXP2101_REG_ADC_ENABLE, adc_en | 0x20);  // VBUS enable
    uint8_t hi = pmic_i2c_read_reg(0x38);
    uint8_t lo = pmic_i2c_read_reg(0x39);
    return (uint16_t)((((uint16_t)hi << 4) | (lo & 0x0F)) * 1.7f);
}
```

### `p4c5_pmic_print_adc()` 输出
```
I PMIC ADC self-test:
I PMU_STATUS_1 = 0x20 (bit0:VBUS bit1:VInBat bit2:VBat)
I PMU_STATUS_2 = 0x15
I VBUS  = 0 mV  (VBUS 不在场 ❌)
I VBAT  = 0 mV  (无电池)
I ALDO4 = reg 0x95 = 0x18 → 2900 mV  (xiaozhi 2.9V)
```

## 8. commit hash

- T19: `7f80e09` (PWRKEY timing)
- T15: `19371a7` (ADC self-test)
- T15b: latest (ADC enable + PMU_STATUS)