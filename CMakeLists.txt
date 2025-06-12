zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_KEYTOGGLE src/pointing/input-processor-keytoggle.c)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)