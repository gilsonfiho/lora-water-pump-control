# Maquinas de estado dos dois nos + Observer. Camada mais alta: conhece as
# interfaces da hal e a camada de enlace, nunca os drivers concretos.
idf_component_register(
    SRCS        "PumpController.cpp" "ReservoirController.cpp"
    INCLUDE_DIRS "include"
    REQUIRES    config protocol hw platform
)
