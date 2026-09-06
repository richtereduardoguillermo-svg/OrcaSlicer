# Warping (Alabeo / Despegue de esquinas)

## Síntomas y Causas
El alabeo ocurre cuando el plástico extruido se enfría y contrae, levantando las esquinas o bordes de la pieza de la superficie de construcción. Es común en materiales como ABS, ASA, PETG y piezas grandes de PLA.
Causas principales:
- Cama caliente a temperatura insuficiente o descalibrada.
- Enfriamiento prematuro (ventilador de capa encendido en las primeras capas o corrientes de aire).
- Falta de superficie de contacto (sin brim / falda).
- Suciedad o falta de adherencia en la placa.

## Parámetros clave de OrcaSlicer (PrintConfig)
- `bed_temperature_initial_layer`: Aumentar en 5-10 °C para mejorar agarre inicial (ej: PETG a 75-80°C, ABS a 100-110°C).
- `bed_temperature`: Temperatura de cama continua.
- `hot_plate_temp` / `textured_plate_temp`: Temperaturas específicas para placas texturadas PEI.
- `disable_fan_first_layers`: Mantener el ventilador apagado al menos las primeras 3 a 5 capas.
- `brim_type`: Cambiar a 2 (Outer brim) o 4 (Outer and inner brim) si no hay sujeción perimetral.
- `brim_width`: Aumentar a 5.0 - 8.0 mm para piezas con tendencia a levantarse.
- `initial_layer_print_height`: Asegurar una buena altura de primera capa (ej: 0.20 mm).
- `initial_layer_speed`: Reducir a 20-30 mm/s para asegurar deposición firme.
