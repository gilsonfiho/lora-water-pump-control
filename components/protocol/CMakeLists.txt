# Quadro, CRC-16 e camada de enlace (ACK/retry). Depende de hal apenas pela
# interface ILoRaRadio -- nunca de um driver concreto.
idf_component_register(
    SRCS        "Packet.cpp" "LinkLayer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES    config hal platform
)
