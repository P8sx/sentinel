idf_component_register(
    SRCS 
        "main.c"
        "io/io.c"
        "io/gate.c"
        "io/io_handler.c"
        "io/ui_handler.c"
        "drivers/i2c.c"
        "drivers/modbus.c"
        "wireless/wifi.c"
        "common/config.c"
    INCLUDE_DIRS 
        "."
)
set_source_files_properties(control.c
    PROPERTIES COMPILE_FLAGS
    -Wattributes
)