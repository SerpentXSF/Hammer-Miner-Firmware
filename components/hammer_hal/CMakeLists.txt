idf_component_register(
SRCS 
    #"eeprom.c"
    "TMP75.c"
    "gpio_input_output.c"
    "internal_sensor.c"
    "EMC2302.c"
    "hal_i2c.c"
    "husb238a.c"
    "TPS546.c"
    "pwm_fan.c"
    "vcore.c"
    "ethernet_init.c"

PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../main"
INCLUDE_DIRS 
    "include"
    "../asic/include"
    "../stratum/include"

REQUIRES
    "esp_adc" 
    "freertos"
    "driver"
    "esp_eth"
    "esp_event"
    "esp_netif"
    "json"
    "tcp_transport"
    idf::app_trace 
)


# Include the header files from "main" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main")

# Include the header files from "main/tasks" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main/tasks")

# target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../asic/")

# target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../stratum/")