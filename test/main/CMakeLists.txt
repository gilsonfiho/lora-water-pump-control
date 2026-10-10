idf_component_register(
    SRCS        "test_main.cpp"
                "test_packet.cpp"
                "test_messages.cpp"
                "test_link_layer.cpp"
                "test_pump_controller.cpp"
                "test_reservoir_controller.cpp"
    INCLUDE_DIRS "."
    REQUIRES    unity config platform protocol core hw
    # Os TEST_CASE se registram por __attribute__((constructor)). Sem
    # WHOLE_ARCHIVE o linker descarta esses objetos (ninguem os referencia) e
    # o Unity nao encontra teste nenhum.
    WHOLE_ARCHIVE
)
