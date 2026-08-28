#include <stdbool.h>

#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "miner.h"
#include "hal_i2c.h"
#include "HUSB238A.h"

static const char *TAG = "HUSB238A";
static i2c_master_dev_handle_t s_husb_dev = NULL;

// I2C底层读写（标准实现）
static esp_err_t write_reg(uint8_t reg, uint8_t data) {
    uint8_t buf[] = {reg, data};
    return i2c_master_transmit(s_husb_dev, buf, 2, 100);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *data) {
    return i2c_master_transmit_receive(s_husb_dev, &reg, 1, data, 1, 100);
}

// 芯片使能（仅操作REG_CONTROL1 bit3，原厂正确流程）
static esp_err_t husb_soft_enable(void) {
    uint8_t val;
    ESP_RETURN_ON_ERROR(read_reg(REG_CONTROL1, &val), TAG, "读CONTROL1失败");
    val |= (1 << ENABLE_BIT);  // 仅置位bit3（使能），保留其他位默认值
    return write_reg(REG_CONTROL1, val);
}

// 初始化（原厂标准配置）
esp_err_t husb238a_init(void) {

    ESP_RETURN_ON_ERROR( bc_i2c_add_device(HUSB_I2C_ADDR, &s_husb_dev, TAG), TAG, "init fail");

    // 软件使能芯片（必须步骤）
    ESP_RETURN_ON_ERROR(husb_soft_enable(), TAG, "enable fail");

    #if 0
    // ====================== 【核心安全流程】 ======================
    // 1. 强制进入Disabled状态 → 芯片不输出任何电压
    husb238a_force_disabled();
    ESP_LOGI(TAG, "🔒 芯片已锁定在Disabled状态，无输出");

    // 2. 在Disabled状态下，强制设置PDO为15V
    husb238a_set_pdo_in_disabled(15);
    ESP_LOGI(TAG, "✅ 已在Disabled状态下强制设置PDO=15V");

    // 3. 发送GO命令（在Disabled状态下预发送）
    write_reg(REG_GO_COMMAND, GO_CMD_EXEC_PDO);

    // 4. 退出Disabled状态 → 芯片直接启动15V，永远不会碰20V！
    husb238a_exit_disabled();
    ESP_LOGI(TAG, "🔓 芯片已解锁，直接输出15V！");
    #endif

    husb238a_gate_close();

    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}

// 读取芯片ID（验证通信）
esp_err_t husb238a_read_id(uint8_t *id) {
    return read_reg(REG_DEVICE_ID, id);
}

// 读取连接状态（原厂正确状态位）
esp_err_t husb238a_get_connect_status(bool *is_attached, bool *is_sink_ready) {
    uint8_t status, type;
    ESP_RETURN_ON_ERROR(read_reg(REG_STATUS, &status), TAG, "读STATUS失败");
    ESP_RETURN_ON_ERROR(read_reg(REG_TYPE, &type), TAG, "读TYPE失败");
    *is_attached = (status & STATUS_ATTACHED) ? true : false;
    *is_sink_ready = (type & TYPE_SINK_READY) ? true : false;
    return ESP_OK;
}

// 设置电压档位（原厂正确流程：选择档位→触发协商）
esp_err_t husb238a_set_voltage(uint8_t volt) {
    uint8_t pdo_val = 0;
    switch (volt) {
        case 5:  pdo_val = PDO_5V;  break;
        case 9:  pdo_val = PDO_9V;  break;
        case 12: pdo_val = PDO_12V; break;
        case 15: pdo_val = PDO_15V; break;
        case 20: pdo_val = PDO_20V; break;
        case 28: pdo_val = PDO_28V; break;
        case 36: pdo_val = PDO_36V; break;
        case 48: pdo_val = PDO_48V; break;
        default:
            ESP_LOGE(TAG, "不支持的电压: %dV", volt);
            return ESP_ERR_INVALID_ARG;
    }

    // 1. 选择目标电压档位
    ESP_RETURN_ON_ERROR(write_reg(REG_SRC_PDO_SEL, pdo_val), TAG, "设置电压档位失败");
    // 2. 触发PD协商
    ESP_RETURN_ON_ERROR(write_reg(REG_GO_COMMAND, GO_CMD_EXEC_PDO), TAG, "触发协商失败");

    //ESP_LOGI(TAG, "已发送 %dV 协商请求", volt);
    return ESP_OK;
}

// 读取协商成功的电压（从CONTRACT_STATUS0解析，原厂标准）
esp_err_t husb238a_get_negotiated_voltage(uint8_t *volt) {
    uint8_t contract;
    ESP_RETURN_ON_ERROR(read_reg(REG_CONTRACT_STATUS0, &contract), TAG, "读协商状态失败");
    uint8_t pd_contract = (contract >> 4) & 0x0F;  // 高4位为PD协商结果

    switch (pd_contract) {
        case 0x01: *volt = 5;  break;
        case 0x02: *volt = 9;  break;
        case 0x03: *volt = 12; break;
        case 0x04: *volt = 15; break;
        case 0x05: *volt = 20; break;
        case 0x0A: *volt = 28; break;
        case 0x0B: *volt = 36; break;
        case 0x0C: *volt = 48; break;
        default: *volt = 0;  // 未协商成功
    }
    return ESP_OK;
}

// 读取VBUS采样电压（原厂REG_VBUS_MEASURE，125mV/LSB）
esp_err_t husb238a_get_vbus_measurement(uint16_t *volt_mv) {
    uint8_t meas_val;
    ESP_RETURN_ON_ERROR(read_reg(REG_VBUS_MEASURE, &meas_val), TAG, "读VBUS采样失败");
    *volt_mv = meas_val * 125;  // 手册明确：1LSB=125mV
    return ESP_OK;
}

// 读取【真正实时电压】
esp_err_t husb238a_get_real_vbus_voltage(uint16_t *volt_mv)
{
    uint8_t vl, vh;
    ESP_RETURN_ON_ERROR(read_reg(REG_PPS_VOL_L, &vl), TAG, "read vol L fail");
    ESP_RETURN_ON_ERROR(read_reg(REG_PPS_VOL_H, &vh), TAG, "read vol H fail");

    // 10位有效数据
    uint16_t val = ((vh & 0x03) << 8) | vl;

    // 公式：3V + val * 20mV
    *volt_mv = 3000 + val * 20;

    return ESP_OK;
}

// 读取故障状态
esp_err_t husb238a_get_fault(uint8_t *fault) {
    return read_reg(REG_SYS_STATUS, fault);
}

// ====================== 【关键】强制进入Disabled状态（不输出电压） ======================
esp_err_t husb238a_force_disabled(void)
{
    uint8_t val;
    ESP_RETURN_ON_ERROR(read_reg(REG_MANUAL, &val), TAG, "read manual fail");
    val |= (1 << MANUAL_DISABLED_BIT); // 置位bit1 → 强制Disabled
    return write_reg(REG_MANUAL, val);
}

// ====================== 【关键】退出Disabled，启动输出 ======================
esp_err_t husb238a_exit_disabled(void)
{
    uint8_t val;
    ESP_RETURN_ON_ERROR(read_reg(REG_MANUAL, &val), TAG, "read manual fail");
    val &= ~(1 << MANUAL_DISABLED_BIT); // 清零bit1 → 退出Disabled
    return write_reg(REG_MANUAL, val);
}

// ====================== 【关键】在Disabled状态下强制设置PDO ======================
esp_err_t husb238a_set_pdo_in_disabled(uint8_t volt)
{
    uint8_t pdo = 0;
    switch (volt) {
        case 5: pdo = PDO_5V; break;
        case 9: pdo = PDO_9V; break;
        case 12: pdo = PDO_12V; break;
        case 15: pdo = PDO_15V; break;
        case 20: pdo = PDO_20V; break;
        default: return ESP_ERR_INVALID_ARG;
    }
    // 在Disabled状态下直接写PDO
    return write_reg(REG_SRC_PDO_SEL, pdo);
}

// ====================== 原厂标准GATE控制函数 ======================
// 强制关闭GATE：VBUS完全无输出，后级绝对安全
esp_err_t husb238a_gate_close(void)
{
    uint8_t val;
    ESP_RETURN_ON_ERROR(read_reg(REG_USER_CFG2, &val), TAG, "读USER CFG2失败");
    val |= (1 << GATE_CTRL_BIT);  // bit5写1，立即关闭GATE
    return write_reg(REG_USER_CFG2, val);
}

// 打开GATE：恢复PD协议自动控制，协商成功后输出电压
esp_err_t husb238a_gate_open(void)
{
    uint8_t val;
    ESP_RETURN_ON_ERROR(read_reg(REG_USER_CFG2, &val), TAG, "读USER CFG2失败");
    val &= ~(1 << GATE_CTRL_BIT); // bit5写0，打开GATE
    return write_reg(REG_USER_CFG2, val);
}

// ==========================
// 【接口1】获取当前适配器协议类型
// ==========================
husb238_protocol_t husb238_get_protocol(void)
{
    uint8_t status1, dpdm_status;

    // 读取 PD 通信状态
    read_reg(REG_STATUS1, &status1);
    bool pd_connected = (status1 & 0x10) != 0; // PD_COMM = BIT4

    if (pd_connected) {
        return PROTOCOL_PD;
    }
    else
    {
        //ESP_LOGW(TAG, "REG_STATUS1 %02x", status1);
    }

    // 非 PD → 读 DPDM 协议
    read_reg(REG_DPDM_STATUS, &dpdm_status);

    if (dpdm_status == 0x03) {
        return PROTOCOL_BC12;
    } else if (dpdm_status == 0x05) {
        return PROTOCOL_QC2;
    }
    else
    {
        //ESP_LOGW(TAG, "REG_DPDM_STATUS %02x", dpdm_status);
    }
    return PROTOCOL_NONE;
}

// ==========================
// 【接口2】读取适配器全部电源能力（电压 + 最大电流）
// ==========================
void husb238_get_pdo_capability(husb238_pdo_t *pdo)
{
    uint8_t reg5v, reg9v, reg12v, reg15v, reg20v;

    read_reg(REG_SRC_PDO_5V,  &reg5v);
    read_reg(REG_SRC_PDO_9V,  &reg9v);
    read_reg(REG_SRC_PDO_12V, &reg12v);
    read_reg(REG_SRC_PDO_15V, &reg15v);
    read_reg(REG_SRC_PDO_20V, &reg20v);

    // DETECT 位 = bit7 (1=支持)
    pdo->support_5v  = (reg5v  & 0x80) ? 1 : 0;
    pdo->support_9v  = (reg9v  & 0x80) ? 1 : 0;
    pdo->support_12v = (reg12v & 0x80) ? 1 : 0;
    pdo->support_15v = (reg15v & 0x80) ? 1 : 0;
    pdo->support_20v = (reg20v & 0x80) ? 1 : 0;

    // 电流值 = bit[5:0] (100mA/单位)
    pdo->max_current_5v  = reg5v  & 0x3F;
    pdo->max_current_9v  = reg9v  & 0x3F;
    pdo->max_current_12v = reg12v & 0x3F;
    pdo->max_current_15v = reg15v & 0x3F;
    pdo->max_current_20v = reg20v & 0x3F;
}

// ==========================
// 【接口3】获取当前协商成功的电压
// ==========================
uint8_t husb238_get_negotiated_voltage(void)
{
    uint8_t contract;
    read_reg(REG_CONTRACT_STATUS0, &contract);

    uint8_t volt_code = (contract >> 4) & 0x0F; // PD_CONTRACT [7:4]

    switch(volt_code) {
        case 0x01: return 5;
        case 0x02: return 9;
        case 0x03: return 12;
        case 0x04: return 15;
        case 0x05: return 20;
        default:   return 0;
    }
}

// ==========================
// 【接口4】获取当前协商成功的电流（单位：mA）
// ==========================
uint16_t husb238_get_negotiated_current(void)
{
    uint8_t curr;
    read_reg(REG_CONTRACT_CURRENT, &curr);
    return (curr * 20) + 500; // 公式：20mA/LSB + 偏移 500mA
}

// ==========================
// 【接口5】打印所有信息（调试神器）
// ==========================
void husb238_print_info(void)
{
    husb238_protocol_t proto = husb238_get_protocol();
    husb238_pdo_t pdo;
    husb238_get_pdo_capability(&pdo);

    uint8_t volt = husb238_get_negotiated_voltage();
    uint16_t curr = husb238_get_negotiated_current();

    const char *proto_str = "Unknown";
    if(proto == PROTOCOL_PD)    proto_str = "USB PD";
    if(proto == PROTOCOL_QC2)   proto_str = "QC 2.0";
    if(proto == PROTOCOL_BC12)  proto_str = "BC1.2";

    printf("=====================================\n");
    printf("HUSB238A 适配器信息\n");
    printf("协议类型: %s\n", proto_str);
    printf("当前协商: %dV, %d mA\n", volt, curr);
    printf("支持电压: 5V:%d | 9V:%d | 12V:%d | 15V:%d \n",
           pdo.support_5v, pdo.support_9v, pdo.support_12v,pdo.support_15v);
    printf("最大电流: 5V=%d00mA | 9V=%d00mA | 12V=%d00mA | 15V=%d00mA\n",
           pdo.max_current_5v, pdo.max_current_9v, pdo.max_current_12v, pdo.max_current_15v);
    printf("=====================================\n");
}


// Read PD adapter capability from HUSB238A registers
void husb238a_get_capability(bool *support_5v, bool *support_9v,
                             bool *support_12v, bool *support_15v)
{
    uint8_t r5v, r9v, r12v, r15v;
    read_reg(0x6A, &r5v);
    read_reg(0x6B, &r9v);
    read_reg(0x6C, &r12v);
    read_reg(0x6D, &r15v);

    *support_5v  = (r5v  & 0x80) ? 1 : 0;
    *support_9v  = (r9v  & 0x80) ? 1 : 0;
    *support_12v = (r12v & 0x80) ? 1 : 0;
    *support_15v = (r15v & 0x80) ? 1 : 0;
}