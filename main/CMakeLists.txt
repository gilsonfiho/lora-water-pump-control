idf_component_register(
    SRCS        "main.cpp" "reservoir_app.cpp" "pump_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES    config core protocol hw power platform esp_driver_uart esp_driver_gpio esp_timer
)
