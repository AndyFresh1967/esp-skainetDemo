set(srcs
    main.c
    speech_commands_action.c
    pca9555.c
    led_state.c
    )

set(requires
    hardware_driver
    nvs_flash
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS include
                       REQUIRES ${requires})

component_compile_options(-w)