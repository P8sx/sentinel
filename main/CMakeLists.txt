idf_component_register(
    SRCS 
        "main.c"
        "io/io.c"
        "io/gate.c"
        "io/control.c"
        "drivers/i2c.c"
        "wireless/wifi.c"
        "wireless/rf.c"
        "common/config.c"
    INCLUDE_DIRS 
        "."
)
set_source_files_properties(control.c
    PROPERTIES COMPILE_FLAGS
    -Wattributes
)