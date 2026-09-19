# Deep sleep e observacao do failover de energia (o chaveamento e em hardware).
idf_component_register(
    SRCS        "PowerManager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES    config hw platform esp_hw_support
)
