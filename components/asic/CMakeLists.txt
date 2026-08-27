idf_component_register(
SRCS 
    "bm1370.c"
    "serial.c"
    "crc.c"
    "common.c"
    "lt0051.c"
    "asic.c"
    "frequency_transition_bmXX.c"
    "pll.c"

INCLUDE_DIRS 
    "include"

REQUIRES 
    "freertos"
    "driver"
    "stratum"
    "a"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -fvisibility=hidden
)

# Include the header files from "main" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main")

# Include the header files from "main/tasks" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main/tasks")