# Stringing (Hilos / Telarañas)

## Síntomas y Causas
Finos filamentos de plástico entre torres o espacios abiertos mientras el cabezal viaja.
Causas principales:
- Retracción insuficiente (distancia muy corta o velocidad muy lenta).
- Temperatura de boquilla excesivamente alta (el filamento fluye por gravedad).
- Velocidad de viaje (`travel_speed`) muy baja.
- Filamento con humedad acumulada.

## Parámetros clave de OrcaSlicer (PrintConfig)
- `retraction_length`: En direct drive aumentar de 0.8 mm a 1.2-1.5 mm; en Bowden de 4 mm a 6 mm.
- `retraction_speed`: Ajustar entre 30 y 45 mm/s.
- `nozzle_temperature`: Reducir en 5-10 °C si el filamento está demasiado líquido.
- `travel_speed`: Incrementar velocidad de traslados a 150-250 mm/s para romper los hilos rápidamente.
- `wipe_distance`: Activar o aumentar distancia de barrido en el cambio de dirección.
