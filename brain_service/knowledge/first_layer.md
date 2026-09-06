# Problemas de Primera Capa (No pega / Aplastamiento)

## Síntomas y Causas
El filamento no se adhiere a la base, se hace una bola en la boquilla o queda transparente/bloqueada.
Causas principales:
- Z-offset o nivelación incorrecta.
- Flujo de primera capa inadecuado.
- Velocidad de primera capa demasiado alta.

## Parámetros clave de OrcaSlicer (PrintConfig)
- `initial_layer_print_height`: Usar 0.20 mm a 0.28 mm para compensar tolerancias de la base.
- `initial_layer_speed`: Reducir a 20-30 mm/s.
- `bed_temperature_initial_layer`: Incrementar 5°C.
- `initial_layer_infill_speed`: Reducir velocidad de relleno de primera capa.
