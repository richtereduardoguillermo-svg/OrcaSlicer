# Checkpoint Fase 3 — Diagnóstico real con contexto de laminado y routing híbrido

**Fecha y hora:** 2026-09-06 03:10 (hora local)  
**Estado:** COMPLETADO CON ÉXITO  
**Rama:** `ai-copilot`  

---

## 1. Qué se hizo

1. **Base de conocimiento técnica (`brain_service/knowledge/*.md`):**
   - Se crearon documentos temáticos concisos con causas físicas y los parámetros exactos de `PrintConfig` (`src/libslic3r/PrintConfig.cpp`):
     - `warping.md`: `bed_temperature_initial_layer`, `disable_fan_first_layers`, `brim_type`, `brim_width`.
     - `stringing.md`: `retraction_length`, `retraction_speed`, `travel_speed`, `nozzle_temperature`.
     - `layer_adhesion.md`: `fan_max_speed`, `fan_min_speed`, `nozzle_temperature`, `layer_height`.
     - `first_layer.md`: `initial_layer_print_height`, `initial_layer_speed`, nivelación y z-offset.

2. **Motor de enrutamiento híbrido (`brain_service/routing.py`):**
   - **Selección de conocimiento**: Filtrado dinámico de archivos Markdown relevantes según términos del mensaje y claves `opt_key` de warnings.
   - **Diagnóstico Local (`confidence: "local"`):**
     - Reglas expertas determinísticas que evalúan el material activo en el contexto (`Generic ABS`, PETG, PLA) y resuelven fallas comunes instantáneamente sin latencia ni dependencia de red.
   - **Diagnóstico Cloud Gemini (`confidence: "cloud"`):**
     - Integración con el CLI oficial `agy -p` para consultas abiertas o complejas.
     - Prompt con inyección de fragmentos de conocimiento, presets activos, warnings y estadísticas, exigiendo respuesta en JSON estricto con claves válidas de `PrintConfig`.

3. **Validación previa del contrato JSON con `curl` (sin tocar C++):**
   - Se prepararon payloads de prueba en `brain_service/tests/`:
     - `ejemplo_warping.json`
     - `ejemplo_stringing.json`
     - `ejemplo_cloud.json`
   - Se ejecutó `brain_service.py` y se verificaron con `curl`, confirmando respuestas 200 OK y el cumplimiento estricto del esquema:
     `{"status": "ok", "diagnosis_text": "...", "proposed_changes": {...}, "confidence": "local"|"cloud", "requires_confirmation": true}`.

4. **Recolección de contexto de laminado en C++ (`AICopilotPanel.cpp`):**
   - En `send_query_to_brain`:
     - **Presets activos**: Extracción desde `wxGetApp().preset_bundle` (`printers`, `filaments`, `prints`).
     - **Diff de configuración**: Recolección de opciones modificadas mediante `current_dirty_options()` y `config.opt_serialize(key)`.
     - **Warnings estructurados**: Llamada a `plater->fff_print().validate(&warnings)`, empaquetando cada `StringObjectException` con su `type`, `message` y `opt_key`.
     - **Estadísticas de laminado**: Lectura de `plater->fff_print().print_statistics()` (`estimated_normal_print_time`, `total_used_filament`, etc.).
     - Despacho asíncrono con `Slic3r::Http::post("http://127.0.0.1:8787/diagnose")`.
     - Desempaque seguro en `on_complete` con `wxGetApp().CallAfter`: muestra la etiqueta del copiloto (`Local` o `Gemini Cloud`), la explicación en lenguaje natural y el listado de parámetros sugeridos.

5. **Compilación incremental y verificación en vivo:**
   - Compilado con `./build_linux.sh -g -s` en **2 minutos 07 segundos**.
   - Verificado en la GUI real con `Sacapuntas (Full 3D).stl` y filamento activo `Generic ABS`:
     - **Prueba Local**: Se consultó `"como soluciono el warping en esta pieza"`. El sistema detectó que el preset cargado era ABS, emitió diagnóstico técnico de contracción y propuso parámetros reales de OrcaSlicer (`bed_temperature_initial_layer: 100`, `brim_type: 2`, `brim_width: 8.0`, `disable_fan_first_layers: 5`).
     - **Prueba Cloud**: Se consultó `"como optimizar detalle en pieza artistica"`. El servicio derivó la consulta a Gemini Cloud vía `agy -p`, respondiendo en segundos con recomendaciones de altura de capa y velocidad (`layer_height: 0.06`, `outer_wall_speed: 40`, `wall_generator: "arachne"`).
   - Ambas respuestas se renderizaron de forma fluida en el chat sin bloquear la GUI ni provocar advertencias en GTK.

---

## 2. Archivos modificados o creados

- `brain_service/knowledge/warping.md` (ampliado)
- `brain_service/knowledge/stringing.md` (ampliado)
- `brain_service/knowledge/layer_adhesion.md` (ampliado)
- `brain_service/knowledge/first_layer.md` (creado)
- `brain_service/routing.py` (motor de enrutamiento local y Gemini CLI `agy`)
- `brain_service/brain_service.py` (soporte completo de `POST /diagnose`)
- `brain_service/tests/ejemplo_warping.json` (payload de prueba)
- `brain_service/tests/ejemplo_stringing.json` (payload de prueba)
- `brain_service/tests/ejemplo_cloud.json` (payload de prueba)
- `src/slic3r/GUI/AICopilotPanel.cpp` (recolección de contexto de Print/Presets y renderizado de diagnóstico)
- `CHECKPOINTS/screenshots/fase3_local_diagnosis.png` (evidencia diagnóstico local)
- `CHECKPOINTS/screenshots/fase3_cloud_diagnosis.png` (evidencia diagnóstico cloud Gemini)
- `CHECKPOINTS/screenshots/fase3_full_gui.png` (evidencia pantalla completa)

---

## 3. Desvíos y notas para el revisor

1. **Seguridad de tipos en strings wxWidgets:** Durante la primera prueba de Fase 3, se identificó que el formateo variádico `wxString::Format("%s", wxString)` al iterar `proposed_changes` podía producir una violación de segmento en compilaciones estrictas de C++ al pasar un objeto `wxString` a una función elipsis C. Se sustituyó por concatenación directa de tipos seguros (`full_msg += "\n • " + key + ": " + val`), garantizando estabilidad absoluta en wxGTK3.
2. **Listo para Fase 4:** Con el contrato estable y los parámetros sugeridos recibiéndose como claves exactas de `PrintConfig`, la base está lista para implementar el panel de visualización de diff y la persistencia de presets derivados (`inherits`) sin mutar los originales.
