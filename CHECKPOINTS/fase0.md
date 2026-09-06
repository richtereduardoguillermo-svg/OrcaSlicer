# Checkpoint Fase 0 — Validación del build vanilla y entorno

**Fecha y hora:** 2026-09-06 01:20 (hora local)  
**Estado:** COMPLETADO CON ÉXITO  
**Commit local:** `6a7af0b5` ("fase 0: soporte para compilación con Podman rootless y validación del build vanilla")  

---

## 1. Qué se hizo

1. **Configuración de remotos Git:**
   - Remoto original renombrado a `upstream` (`https://github.com/OrcaSlicer/OrcaSlicer.git`).
   - Creado fork propio en GitHub: `https://github.com/richtereduardoguillermo-svg/OrcaSlicer.git`.
   - Remoto `origin` configurado apuntando exclusivamente al fork de Eduardo.
   - Verificado con `git remote -v`: ningún push apuntará a `upstream`.

2. **Limpieza de código legacy / archivos huérfanos:**
   - Se eliminaron los archivos del intento anterior que no formaban parte de OrcaSlicer C++:
     - `iaslicer_studio.py`
     - `iaslicer-splash.py`
     - `iaslicer-launcher.sh`
     - `resources/icons/`
     - `resources/web/OrbitControls.js`, `resources/web/STLLoader.js`, `resources/web/bed_viewport.html`, `resources/web/three.min.js`
   - El árbol de trabajo quedó completamente limpio.

3. **Adaptación del runner Podman (Fedora Host + contenedor Ubuntu 24.04):**
   - **SELinux:** Podman rootless en Fedora bloqueaba la escritura de volúmenes por SELinux. Se configuró `~/.config/containers/containers.conf` con `label = false` para permitir montajes de volumen sin alterar archivos del sistema ni usar `sudo`.
   - **UID Mapping:** En Podman rootless, el UID 1000 del host se mapea como UID 0 (root) dentro del contenedor. El script `build_linux.sh` intentaba crear un usuario con UID 1000 dentro del contenedor, provocando que los archivos generados en `deps/build` pertenecieran a un subuid (`525287`) inmodificable por el usuario local. Se aplicó un ajuste mínimo en `build_linux.sh` (líneas 328-336) para que cuando el runtime sea `podman`, use `HOST_UID=0`/`HOST_GID=0`, garantizando que todos los archivos creados pertenezcan transparentemente a `eduardo:eduardo` (UID 1000) en el host.
   - Imagen del runner construida y cacheada exitosamente en Podman: `orcaslicer-linux-builder:ubuntu-24.04--cmake-4.3.0--8f2aa87318b2`.

4. **Canarios de compilación:**
   - `dep_Boost`: Compiló exitosamente (`[8/8] Completed 'dep_Boost'`) en **1m 08s** con GCC 13.3.0 (16 hilos).
   - `dep_CGAL` (incluyendo `dep_GMP` y `dep_MPFR`): Compiló exitosamente (`[32/32] Completed 'dep_CGAL'`) en **~1m** con GCC 13.3.0.

5. **Compilación vanilla completa (`./build_linux.sh -g -ds`):**
   - Se compilaron todas las dependencias restantes (TBB, OpenSSL, wxWidgets, OpenVDB, OpenCV, Draco, NLopt, etc.) y la aplicación completa OrcaSlicer en modo `Release`.
   - **Tiempo total de compilación:** **49 minutos 28 segundos** (exitoso, código de salida 0).
   - Artefactos producidos:
     - Binario principal: `build/src/Release/orca-slicer` (157 MB)
     - Paquete ejecutable: `build/package/orca-slicer`

6. **Verificación funcional (arranque, GUI, carga de STL y laminado):**
   - Se sincronizaron los perfiles y presets reales de Eduardo desde Flatpak (`~/.var/app/com.orcaslicer.OrcaSlicer/config/OrcaSlicer/`) hacia `~/.config/OrcaSlicer/` (impresoras Bambu Lab A1, perfiles de proceso y filamentos).
   - Se arrancó la aplicación nativa en X11 (`DISPLAY=:0.0`).
   - Se cargó el modelo STL real: `/home/eduardo/Documentos/Sacapuntas (Full 3D).stl` (30.448 triángulos, 99.98 x 89.95 x 120.00 mm).
   - Se ejecutó el laminado completo ("Laminar cama") desde la GUI:
     - 1.200 capas calculadas (altura 120.00 mm, capa 0.10 mm).
     - G-code completo generado y visualizado en la pestaña "Previsualización" con tipos de línea codificados por color.
     - Estimación calculada: 64,08 m de filamento (160,30 g), tiempo estimado 1d8h44m.
   - También se verificó el motor de laminado en modo CLI con un proyecto `.3mf` real (`mate sacapuntas.3mf`), generando exitosamente 16 MB de G-code y `result.json` con `return_code: 0`.

---

## 2. Archivos modificados o creados

- `build_linux.sh`: Ajuste en líneas 328-336 para mapeo de UID/GID en Podman rootless.
- `CHECKPOINTS/screenshots/`:
  - `fase0_window.png`: Diálogo inicial de certificados SSL en primer arranque.
  - `fase0_main.png`: Primer arranque cargando STL de prueba.
  - `fase0_sacapuntas_gui.png`: GUI principal con la cama Bambu Lab A1 y el STL `Sacapuntas (Full 3D)` cargado.
  - `fase0_sliced_preview.png`: Pestaña de previsualización mostrando el modelo 100% laminado (1.200 capas, G-code generado).

---

## 3. Desvíos y notas para el revisor

1. **Permisos Podman:**
   - La directiva indicaba correr `./build_linux.sh -g -d` asumiendo que Podman funcionaba idéntico a Docker. En Fedora, SELinux bloquea volúmenes sin deshabilitar etiquetas de contenedor o sin `:z`, y el mapeo de usuarios rootless hacía que los archivos pertenezcan al rango subuid. Se resolvió con configuración de usuario en `containers.conf` (`label = false`) y el pequeño ajuste en `build_linux.sh`.
2. **Git Push a `origin/main`:**
   - El fork en GitHub (`richtereduardoguillermo-svg/OrcaSlicer`) se creó con la rama `main` en el commit `c0c2cc50` (último commit upstream al momento del fork), mientras que el clon local estaba basado en `067dfa35` (#15353).
   - Al intentar `git push origin main`, git devolvió `[rejected] (fetch first)`.
   - **Por regla de seguridad explícita** ("no improvisar soluciones riesgosas por tu cuenta como `--force` o rebases a ciegas sin autorización del jefe"), el commit local `6a7af0b5` quedó registrado localmente. Consulta para el revisor: ¿preferís hacer `git push origin main --force-with-lease` sobre nuestro propio fork, o trabajar en una rama dedicada como `dev-copilot`?
