idf_component_register(
    SRCS        "main.cpp" "reservoir_app.cpp" "pump_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES    config core protocol hal power platform driver
)
