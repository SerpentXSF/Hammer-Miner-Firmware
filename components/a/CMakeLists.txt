idf_component_register(
INCLUDE_DIRS 
    "include"

REQUIRES 
    "asic"
)

add_library(a STATIC IMPORTED)

set_property(TARGET a PROPERTY IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/liba.a
)

target_link_libraries(${COMPONENT_LIB} INTERFACE a)