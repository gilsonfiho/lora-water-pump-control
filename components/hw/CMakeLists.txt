# Interfaces de hardware + drivers concretos (E220 por UART, rele, boias, ADC).
# E o unico componente que fala com os drivers do ESP-IDF.
idf_component_register(
    SRCS        "E220Radio.cpp" "SX127xRadio.cpp" "AdcBatteryMonitor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES    config platform esp_driver_uart esp_driver_gpio esp_adc
)
