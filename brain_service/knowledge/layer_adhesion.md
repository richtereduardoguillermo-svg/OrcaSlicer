# Adhesión entre Capas (Delaminación / Fragilidad)

## Síntomas y Causas
Las capas intermedias se separan con facilidad o la pieza se rompe por las uniones horizontales bajo tensión mecánica.
Causas principales:
- Temperatura de extrusión demasiado baja (la nueva capa no funde la anterior).
- Exceso de ventilación de capa (enfría la superficie antes de que se produzca la unión molecular).
- Velocidad de impresión muy alta (tiempo de contacto térmico insuficiente).
- Subextrusión.

## Parámetros clave de OrcaSlicer (PrintConfig)
- `nozzle_temperature`: Aumentar en 5-10 °C para favorecer la fusión entre capas.
- `fan_max_speed`: Limitar la velocidad máxima del ventilador (ej: en PETG máx 30-50%, en ABS 0-15%).
- `fan_min_speed`: Reducir la velocidad mínima del ventilador.
- `inner_wall_speed` / `outer_wall_speed`: Reducir velocidades de impresión.
- `layer_height`: Disminuir la altura de capa (menor altura = mayor área de contacto relativa).
