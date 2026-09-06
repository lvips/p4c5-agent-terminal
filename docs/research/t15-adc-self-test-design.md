# T15 设计规格：AXP2101 内部 ADC 自测电源链路

> **版本**：v1.0（2026-09-06，CCB T15 设计）
> **目标**：通过 I2C 读 AXP2101 内部 ADC 自证电源状态，不用万用表
> **执行者**：CCA（需要修改 `hardware/` 下代码）
> **前置**：现有 `p4c5_pmic_get_vbat_mv()` 和 `axp2101_get_vbus_voltage_mv()` 可用

---

## 1. 现状分析

### 1.1 已有能力

| API | 位置 | 功能 |
|---|---|---|
| `p4c5_pmic_get_vbat_mv()` | `p4c5_pmic.cc` L443 | VBAT ADC 读取 ✅ |
| `p4c5_pmic_get_die_temp_x10()` | `p4c5_pmic.cc` L465 | 芯片温度 ✅ |
| `p4c5_pmic_dump_all_regs()` | `p4c5_pmic.cc` L531 | 13 寄存器 dump ✅ |
| `p4c5_pmic_read_reg(0x95)` | `p4c5_pmic.cc` L508 | ALDO4 寄存器原始读取 ✅ |
| `axp2101_get_vbus_voltage_mv()` | `axp2101.c` L555 | VBUS ADC（底层已有）✅ |
| `axp2101_get_vsys_voltage_mv()` | `axp2101.c` L376 | VSYS ADC（底层已有）✅ |
| `axp2101_enable_pmu_adc_channels()` | `axp2101.c` L339 | ADC 通道使能（init 时已调用）✅ |

### 1.2 缺失能力

| 缺失 | 说明 |
|---|---|
| `p4c5_pmic_get_vbus_mv()` | VBUS 读取未封装到 p4c5_pmic 层 |
| `p4c5_pmic_get_vsys_mv()` | VSYS 读取未封装 |
| 综合打印函数 | 无 "一行看全电源状态" 的 API |
| app_main.c 调用 | 启动流程和 heartbeat 中无 ADC 打印 |

---

## 2. 设计方案

### 2.1 p4c5_pmic.h — 新增 API

```c
/* ── 电源自测（T15）── */

typedef struct {
    uint16_t vbus_mv;    /* VBUS USB 输入电压 (mV)，期望 5000 */
    uint16_t vsys_mv;    /* VSYS 系统主轨电压 (mV)，期望 3300+ */
    uint16_t vbat_mv;    /* VBAT 电池电压 (mV)，无电池=0 */
    uint8_t  aldo4_reg;  /* ALDO4 reg 0x95 回读值 (0x1D=3.4V, 0x18=2.9V) */
    int16_t  temp_x10;   /* 芯片温度 °C×10 */
} p4c5_pmic_adc_t;

/**
 * @brief 读取 AXP2101 内部 ADC 全部通道
 *
 * 通过 I2C 读 0x34-0x3D ADC 寄存器，
 * 不需要万用表即可验证电源链路。
 *
 * @param[out] adc  输出结构体
 * @return ESP_OK 成功
 */
esp_err_t p4c5_pmic_read_adc(p4c5_pmic_adc_t *adc);

/**
 * @brief 打印 ADC 读数到 ESP_LOGI
 *
 * 输出格式：
 *   PMIC ADC: VBUS=4980mV VSYS=3310mV VBAT=0mV ALDO4=reg0x1D(3400mV) TEMP=42.3C
 *
 * @return ESP_OK
 */
esp_err_t p4c5_pmic_print_adc(void);
```

### 2.2 p4c5_pmic.cc — 新增实现

```c
esp_err_t p4c5_pmic_read_adc(p4c5_pmic_adc_t *adc)
{
    if (!s_initialized || !adc) return ESP_ERR_INVALID_STATE;

    /* VBUS: 调用底层 axp2101 API（已在 init 时使能 ADC 通道） */
    uint16_t vbus = 0;
    axp2101_get_vbus_voltage_mv(&vbus);
    adc->vbus_mv = vbus;

    /* VSYS: 调用底层 axp2101 API */
    uint16_t vsys = 0;
    axp2101_get_vsys_voltage_mv(&vsys);
    adc->vsys_mv = vsys;

    /* VBAT: 使用已有 p4c5_pmic API */
    adc->vbat_mv = p4c5_pmic_get_vbat_mv();

    /* ALDO4: 直接读 reg 0x95 */
    adc->aldo4_reg = pmic_i2c_read_reg(0x95);

    /* Temperature: 使用已有 API */
    adc->temp_x10 = p4c5_pmic_get_die_temp_x10();

    return ESP_OK;
}

esp_err_t p4c5_pmic_print_adc(void)
{
    p4c5_pmic_adc_t adc;
    esp_err_t err = p4c5_pmic_read_adc(&adc);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PMIC ADC read failed: %s", esp_err_to_name(err));
        return err;
    }

    float aldo4_mv = 500.0f + adc.aldo4_reg * 100.0f;
    float temp_c = adc.temp_x10 / 10.0f;

    ESP_LOGI(TAG, "PMIC ADC: VBUS=%umV VSYS=%umV VBAT=%umV "
                  "ALDO4=reg0x%02X(%.0fmV) TEMP=%.1fC",
             adc.vbus_mv, adc.vsys_mv, adc.vbat_mv,
             adc.aldo4_reg, aldo4_mv, temp_c);
    return ESP_OK;
}
```

### 2.3 app_main.c — 调用点

```c
// 在 [1/5] PMIC init 之后：
p4c5_pmic_print_adc();  // 初始化后立即验证电源

// 在 [4/5] 4G init 之前（如果有的话）：
p4c5_pmic_print_adc();  // 4G 上电前确认 ALDO4

// 在 heartbeat 循环中（每 10 次）：
if (++hb_count % 10 == 0) {
    p4c5_pmic_print_adc();  // 定期监控
}
```

---

## 3. ADC 寄存器规格（引用 datasheet）

### 3.1 VBAT ADC（0x34/0x35）

**来源**：AXP2101 规格书 §6.13.2.30-31, p.40

| 寄存器 | 位 | 说明 |
|---|---|---|
| 0x34 | [7:0] | VBAT [13:6] |
| 0x35 | [7:5] | VBAT [5:3], [4:0]=RO |

**公式**：`Vbat(mV) = (0x34 << 5) | (0x35 >> 3)`，1mV/LSB
**现有实现**：`p4c5_pmic.cc` L451 — 已正确实现

### 3.2 VBUS ADC（0x38/0x39）

**来源**：AXP2101 规格书 §6.13.2.34-35, p.41

| 寄存器 | 位 | 说明 |
|---|---|---|
| 0x38 | [7:0] | VBUS [13:6] |
| 0x39 | [7:5] | VBUS [5:3], [4:0]=RO |

**公式**：`Vbus(mV) = (0x38 << 5) | (0x39 >> 3)`，1mV/LSB
**底层 API**：`axp2101_get_vbus_voltage_mv()` at `axp2101.c` L555

### 3.3 VSYS ADC（0x3A/0x3B）

**来源**：AXP2101 规格书 §6.13.2.36-37, p.41

| 寄存器 | 位 | 说明 |
|---|---|---|
| 0x3A | [7:0] | VSYS [13:6] |
| 0x3B | [7:5] | VSYS [5:3], [4:0]=RO |

**公式**：`Vsys(mV) = (0x3A << 5) | (0x3B >> 3)`，1mV/LSB
**底层 API**：`axp2101_get_vsys_voltage_mv()` at `axp2101.c` L376

### 3.4 ADC 使能（0x30）

**来源**：AXP2101 规格书 §6.13.2.29, p.39-40

| Bit | 通道 | 说明 |
|---|---|---|
| [0] | Battery voltage | VBAT ADC enable |
| [1] | TS pin measure | TS ADC enable |
| [2] | VBUS voltage | VBUS ADC enable |
| [3] | System voltage | VSYS ADC enable |
| [4] | Die temperature | 温度 ADC enable |
| [5] | General purpose | 通用 ADC enable |

**现有代码**：`axp2101_enable_pmu_adc_channels()` (L339) 已在 init 时调用，使能了 VBUS + VBAT + VSYS + 温度通道。

---

## 4. 预期输出

### 4.1 有 USB 充电器、无电池

```
I (xxxx) p4c5_pmic: PMIC ADC: VBUS=4980mV VSYS=3310mV VBAT=0mV ALDO4=reg0x1D(3400mV) TEMP=42.3C
```

### 4.2 诊断判据

| 读数 | 正常范围 | 异常判据 | 可能原因 |
|---|---|---|---|
| VBUS | 4800-5200mV | < 4500mV | USB 线/充电器问题 |
| VSYS | 3200-3400mV | < 3000mV | AXP2101 DCDC1 异常 |
| VBAT | 0（无电池）或 3700-4200mV | 有电池但 =0 | 电池连接断开 |
| ALDO4 reg | 0x1D (3.4V) | 其他值 | 寄存器被覆盖 |
| TEMP | 25-60°C | > 70°C | 散热问题 |

---

## 5. 对 CCA 的实现指令

**需要修改的文件**（3 个）：

1. **`p4c5_pmic.h`** — 添加 `p4c5_pmic_adc_t` 结构体 + 两个函数声明（§2.1）
2. **`p4c5_pmic.cc`** — 添加 `p4c5_pmic_read_adc()` + `p4c5_pmic_print_adc()` 实现（§2.2）
3. **`app_main.c`** — 在 PMIC init 后和 heartbeat 中添加 `p4c5_pmic_print_adc()` 调用（§2.3）

**约束**：
- ❌ 不改 ALDO4 实际值
- ❌ 不改 PWRKEY 时序
- ❌ 不改其他模块
- ✅ 只添加新的 ADC 读取 + 打印函数
- ✅ 复用已有 `axp2101_get_vbus_voltage_mv()` 等底层 API

---

**来源**：
- AXP2101 规格书 Rev1.4: `C3036461_*.PDF` p.39-41 (§6.13.2.29-37)
- 现有代码: `p4c5_pmic.cc` L443-476, `axp2101.c` L339-360, L555-557
- 底层 API: `axp2101.h` L366-381
