idf_component_register(
    SRCS
        "isleapp.cpp"
        "islefiles.cpp"

        "main.c"
    REQUIRES iniparser lego1 miniwin
    PRIV_REQUIRES pthread spi_flash
    PRIV_INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-address
    -Wno-delete-non-virtual-dtor
    -Wno-maybe-uninitialized
    -Wno-overloaded-virtual
    -Wno-reorder
    -Wno-switch
    -Wno-unknown-pragmas
    -Wno-unused-label
)
