idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "../include" "../../asic/include" "../../../main"
    REQUIRES unity 
    PRIV_REQUIRES "driver"
)