idf_component_register(
    SRCS        "Clock.cpp" "Nonce.cpp"
    INCLUDE_DIRS "include"
    REQUIRES    esp_timer
)
