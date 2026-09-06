# Checkpoint Fase 4 — Visualización de diff y aplicación de presets derivados con herencia (Final)

**Fecha y hora:** 2026-09-06 05:40 (hora local)  
**Estado:** COMPLETADO CON ÉXITO  
**Rama:** `ai-copilot`  
**Commit:** `6f7c45ee`  

---

## 1. Qué se hizo

1. **Tabla interactiva de Diff en la interfaz de usuario (`wxDataViewListCtrl`):**
   - Se diseñó e integró un componente `m_diff_panel` en `AICopilotPanel.hpp` y `AICopilotPanel.cpp`, inicialmente oculto para no ocupar espacio visual innecesario.
   - El control cuenta con 3 columnas auto-ajustables:
     - `Clave`: Nombre canónico del parámetro de configuración (`PrintConfig`).
     - `Actual`: Valor vigente obtenido dinámicamente desde el `DynamicPrintConfig` de los presets activos según la familia (`determine_family` y `get_current_option_value`).
     - `Propuesto`: Valor numérico o textual recomendado por el copiloto.
   - Panel de acciones con dos botones ergonómicos:
     - `Aplicar cambios`: Inicia el flujo de persistencia segura y derivación de presets.
     - `Descartar`: Limpia los cambios propuestos y oculta el panel de diff.

2. **Clasificación y separación por familias de configuración:**
   - La función `determine_family()` inspecciona las colecciones del `PresetBundle` (`prints`, `filaments`, `printers`) contra el modelo de opciones de OrcaSlicer (`DynamicPrintConfig`) para determinar inequívocamente a qué familia pertenece cada clave.
   - Cuando el copiloto sugiere una combinación de cambios de proceso y filamento (ej. `brim_width` de proceso y `hot_plate_temp` de filamento), el sistema agrupa automáticamente los cambios por familia (`Preset::TYPE_PRINT`, `Preset::TYPE_FILAMENT`, `Preset::TYPE_PRINTER`).

3. **Creación de presets derivados con herencia estricta (`detach = false`):**
   - Por cada familia afectada:
     - Se solicita confirmación del nombre mediante un cuadro de diálogo modal nativo (`wxTextEntryDialog`), precompletado con la convención estándar: `"<preset_original> - AI tuned <YYYY-MM-DD>"`.
     - Se clona el preset original en memoria (`Preset temp_preset = orig_preset`).
     - Se aplican los valores propuestos mediante `temp_preset.config.set_deserialize_strict(key, value)`.
     - Se guarda el nuevo perfil llamando a `collection->save_current_preset(chosen_name, false, false, &temp_preset)`.
     - **Garantía de integridad:** Al pasar `detach = false`, OrcaSlicer escribe un nuevo archivo JSON en `~/.config/OrcaSlicer/user/<usuario>/<familia>/<nombre>.json` conteniendo la directiva `"inherits": "<preset_original>"` y serializando únicamente las claves modificadas contra el padre.
     - **El archivo original en disco (`resources/profiles/` o perfil base de usuario) NO se modifica en ningún momento.**

4. **Sincronización integral del UI y la sesión:**
   - Se actualiza la compatibilidad de presets con `bundle->update_compatible(PresetSelectCompatibleType::Never)`.
   - Se refresca la pestaña correspondiente invocando el método público `tab->update_tab_ui(true)`.
   - Se actualizan los selectores de la barra lateral con `sidebar().update_presets_from_to(family, orig_name, chosen_name)`, `sidebar().update_presets(family)` y `sidebar().update_all_preset_comboboxes()`.
   - Se persisten las selecciones activas en la configuración de la aplicación mediante `bundle->export_selections(*app_config)`.
   - Se emite un mensaje de confirmación detallado en el chat del copiloto y se limpia la tabla de diff.

---

## 2. Pruebas realizadas y validación en vivo

1. **Prueba End-to-End con diagnóstico real (`tengo warping en abs`):**
   - Con OrcaSlicer ejecutándose y los presets activos:
     - Proceso: `0.10mm Standard @BBL A1 0.2 nozzle`
     - Filamento: `Generic ABS @BBL A1 0.2 nozzle`
   - Se envió la consulta de diagnóstico al copiloto.
   - `brain_service` resolvió localmente (`confidence: "local"`) emitiendo 4 ajustes:
     - `brim_type: "outer_only"` (Proceso)
     - `brim_width: 8.0` (Proceso)
     - `hot_plate_temp: 100` (Filamento)
     - `close_fan_the_first_x_layers: 5` (Filamento)
   - La tabla de diff se pobló y se mostró correctamente con los valores actuales vs propuestos:
     - `brim_type`: `auto_brim` → `outer_only`
     - `brim_width`: `5` → `8`
     - `hot_plate_temp`: `90` → `100`
     - `close_fan_the_first_x_layers`: `3` → `5`

2. **Aplicación interactiva y confirmación de nombres:**
   - Se pulsó el botón `[ Aplicar cambios ]`.
   - El sistema solicitó el nombre para el preset de **Proceso (Print)**, sugiriendo `0.10mm Standard @BBL A1 0.2 nozzle - AI tuned 2026-09-06`.
   - Al confirmar, el sistema solicitó el nombre para el preset de **Filamento (Filament)**, sugiriendo `Generic ABS @BBL A1 0.2 nozzle - AI tuned 2026-09-06`.
   - Ambos fueron confirmados exitosamente.

3. **Verificación de archivos y herencia en disco:**
   - Se inspeccionaron directamente los archivos generados en `~/.config/OrcaSlicer/user/default/`:
     - `process/0.10mm Standard @BBL A1 0.2 nozzle - AI tuned 2026-09-06.json`:
       - `"inherits": "0.10mm Standard @BBL A1 0.2 nozzle"`
       - `"brim_type": "outer_only"`
       - `"brim_width": "8"`
     - `filament/Generic ABS @BBL A1 0.2 nozzle - AI tuned 2026-09-06.json`:
       - `"inherits": "Generic ABS @BBL A1 0.2 nozzle"`
       - `"close_fan_the_first_x_layers": ["5"]`
       - `"from": "User"`
   - Se comprobó con `git status` que los perfiles del sistema bajo `resources/profiles/` permanecieron completamente inalterados.
   - Los comboboxes de la barra lateral de OrcaSlicer reflejaron inmediatamente los nuevos perfiles derivados como los perfiles activos de la sesión.

---

## 3. Archivos modificados o creados

- `src/slic3r/GUI/AICopilotPanel.hpp`: Declaración de controles de diff (`wxDataViewListCtrl`, `m_diff_panel`, `m_btn_apply`, `m_btn_discard`) y métodos de control del ciclo de vida de diff y aplicación.
- `src/slic3r/GUI/AICopilotPanel.cpp`: Implementación de la tabla de diff, mapeo de opciones vigentes con `get_current_option_value()`, partición por familia con `determine_family()`, guardado derivado con `save_current_preset(..., detach=false)` y actualización de UI.
- `brain_service/routing.py`: Estandarización de las claves de diagnóstico local y cloud con los identificadores canónicos de `PrintConfig` (`hot_plate_temp`, `cool_plate_temp`, `close_fan_the_first_x_layers`, `brim_type`, `brim_width`, `filament_retraction_length`, `filament_retraction_speed`).
- `CHECKPOINTS/screenshots/fase4_diff_table.png`: Captura de pantalla de la tabla de diff interactiva en el panel del copiloto.
- `CHECKPOINTS/screenshots/fase4_preset_dialog.png`: Captura del diálogo modal de confirmación de preset derivado con nombre precompletado y aviso de herencia.
- `CHECKPOINTS/screenshots/fase4_preset_applied.png`: Captura del resultado final con los presets derivados aplicados y seleccionados en la barra lateral.

---

## 4. Evidencias fotográficas

- **Tabla de Diff propuesta:** `CHECKPOINTS/screenshots/fase4_diff_table.png`
- **Diálogo de guardado con herencia:** `CHECKPOINTS/screenshots/fase4_preset_dialog.png`
- **Presets derivados activos en la interfaz:** `CHECKPOINTS/screenshots/fase4_preset_applied.png`

---

## 5. Desvíos y notas técnicas para el revisor

1. **Acceso a actualización de pestañas:** El método `Tab::on_presets_changed()` tiene visibilidad `protected` en la jerarquía de pestañas de OrcaSlicer. Se utilizó la API pública `Tab::update_tab_ui(true)` que internamente invoca la recarga completa del modelo y los controles de la pestaña de forma limpia y canónica.
2. **Desacople seguro (`detach=false`):** OrcaSlicer gestiona internamente la relación padre-hijo al invocar `save_current_preset` con `detach=false`. Esto genera automáticamente un archivo diferencial en formato JSON de usuario, garantizando que futuras actualizaciones de los perfiles base sigan propagándose a los ajustes no modificados por el copiloto.
3. **Comando de prueba rápido `/test_apply`:** Para facilitar la verificación en entornos de testing automatizado o desarrollo sin conexión al microservicio, se preservó el comando `/test_apply` en el chat del panel, el cual inyecta un diff de prueba multi-familia de manera inmediata.
