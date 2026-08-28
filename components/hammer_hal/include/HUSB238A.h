#ifndef __HUSB238A_H__
#define __HUSB238A_H__

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 硬件配置（根据实际接线修改）
#define HUSB_I2C_NUM           I2C_NUM_0
#define HUSB_I2C_SDA_PIN       1
#define HUSB_I2C_SCL_PIN       2
#define HUSB_INT_PIN           3
#define HUSB_I2C_ADDR          0x42  // PIN8串900kΩ接GND

// 原厂明确的寄存器（来自HUSB238A Register Information Rev0.1）
#define REG_DEVICE_ID          0x00    // 芯片ID
#define REG_CONTROL1           0x02    // 使能+防抖配置
#define REG_MANUAL             0x03    // 【关键】手动控制寄存器
#define REG_USER_CFG2          0x0E
#define REG_GO_COMMAND         0x18    // 命令触发
#define REG_SRC_PDO_SEL        0x19    // 电压档位选择
#define REG_STATUS             0x63    // 核心状态
#define REG_STATUS1            0x64
#define REG_TYPE               0x65    // 连接类型
#define REG_DPDM_STATUS        0x66
#define REG_CONTRACT_STATUS0   0x67    // 协商结果（电压档位）
#define REG_CONTRACT_CURRENT   0x68
#define REG_SRC_PDO_5V         0x6A
#define REG_SRC_PDO_9V         0x6B
#define REG_SRC_PDO_12V        0x6C
#define REG_SRC_PDO_15V        0x6D
#define REG_SRC_PDO_20V        0x6E
#define REG_VBUS_MEASURE       0x87    // VBUS电压采样（125mV/LSB）
#define REG_SYS_STATUS         0x88    // 真正的系统故障/状态寄存器（替代REG_FAULT）
#define REG_PPS_VOL_L          0x89    // 实时电压低8位
#define REG_PPS_VOL_H          0x8B    // 实时电压高2位
#define REG_PPS_CUR            0x8A    // 实时电流

#define MANUAL_DISABLED_BIT    1       // bit1=1 → 强制Disabled
#define GATE_CTRL_BIT          5       // bit5 = EN_FAULTIN，GATE控制位
// 位定义（原厂标准）
#define ENABLE_BIT             3       // REG_CONTROL1 bit3=使能芯片
#define GO_CMD_EXEC_PDO        0x01    // 执行电压协商
#define STATUS_ATTACHED        BIT(0)  // 0x63 bit0=已连接
#define TYPE_SINK_READY        BIT(4)  // 0x65 bit4=Sink就绪

// 电压档位（REG_SRC_PDO_SEL 高5位值，原厂定义）
#define PDO_5V                 0x08    // 00001b <<3
#define PDO_9V                 0x10    // 00010b <<3
#define PDO_12V                0x18    // 00011b <<3
#define PDO_15V                0x20    // 00100b <<3
#define PDO_20V                0x28    // 00101b <<3
#define PDO_28V                0x60    // 11000b <<3
#define PDO_36V                0x68    // 11010b <<3
#define PDO_48V                0x70    // 11100b <<3

// ==========================
// 协议类型枚举
// ==========================
typedef enum {
    PROTOCOL_NONE    = 0,  // 无协议
    PROTOCOL_PD      = 1,  // USB PD 协议
    PROTOCOL_QC2     = 2,  // QC 2.0
    PROTOCOL_BC12    = 3,  // BC1.2
} husb238_protocol_t;

// ==========================
// PDO 电源能力结构体
// ==========================
typedef struct {
    bool    support_5v;    // 是否支持 5V
    bool    support_9v;    // 是否支持 9V
    bool    support_12v;   // 是否支持 12V
    bool    support_15v;   // 是否支持 15V
    bool    support_20v;   // 是否支持 20V

    uint8_t max_current_5v; // 5V 最大电流 (100mA/单位)
    uint8_t max_current_9v;
    uint8_t max_current_12v;
    uint8_t max_current_15v;
    uint8_t max_current_20v;
} husb238_pdo_t;

// 函数接口
esp_err_t husb238a_init(void);
esp_err_t husb238a_read_id(uint8_t *id);
esp_err_t husb238a_get_connect_status(bool *is_attached, bool *is_sink_ready);
esp_err_t husb238a_set_voltage(uint8_t volt);  // 仅支持电压档位设置（无电流采样功能）
esp_err_t husb238a_get_negotiated_voltage(uint8_t *volt);  // 读取协商成功的电压
esp_err_t husb238a_get_vbus_measurement(uint16_t *volt_mv);  // 读取VBUS采样电压
esp_err_t husb238a_get_real_vbus_voltage(uint16_t *volt_mv); // 读取【真正实时电压】
esp_err_t husb238a_get_fault(uint8_t *fault);

esp_err_t husb238a_force_disabled(void);
esp_err_t husb238a_exit_disabled(void);
esp_err_t husb238a_set_pdo_in_disabled(uint8_t volt);

esp_err_t husb238a_gate_close(void);
esp_err_t husb238a_gate_open(void);

husb238_protocol_t husb238_get_protocol(void);
void husb238_get_pdo_capability(husb238_pdo_t *pdo);
uint8_t husb238_get_negotiated_voltage(void);
uint16_t husb238_get_negotiated_current(void);
void husb238_print_info(void);

void husb238a_get_capability(bool *support_5v, bool *support_9v,
                             bool *support_12v, bool *support_15v);

#ifdef __cplusplus
}
#endif

#endif