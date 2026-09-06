"""
gemini_client - Cliente directo a la API REST de Gemini (generativelanguage.googleapis.com).

Reemplaza tanto el LLM local (abandonado: un modelo de 4B corriendo local no
seguía de forma confiable las reglas del prompt, que ya son varias) como el
CLI 'agy -p' (lento: 11-35s, con timeouts frecuentes, y frágil para parsear
JSON de su stdout). Este cliente usa response_mime_type=application/json de
la API real, que garantiza JSON válido sin necesidad de bloques ```json ni
regex para extraerlo.

Sigue la misma regla del proyecto: solo stdlib de Python (urllib), sin
dependencias pip externas (nada de google-generativeai ni requests).
"""

import json
import logging
import os
import re
import time
import urllib.error
import urllib.request
from pathlib import Path

MODEL = "gemini-3.1-flash-lite"
API_URL = f"https://generativelanguage.googleapis.com/v1beta/models/{MODEL}:generateContent"

# La key vive en otro proyecto (Vader animatronic), no se copia acá para no
# duplicar secretos. Si esa ruta no existe (ej. otra máquina), simplemente no
# hay API key disponible y route_query cae al fallback genérico.
ENV_FILE_FALLBACK = Path.home() / "vader_animatronic" / "config" / ".env"

_cached_api_key = None


def _load_api_key() -> str:
    global _cached_api_key
    if _cached_api_key:
        return _cached_api_key

    key = os.environ.get("GEMINI_API_KEY")
    if not key and ENV_FILE_FALLBACK.exists():
        try:
            for line in ENV_FILE_FALLBACK.read_text(encoding="utf-8").splitlines():
                m = re.match(r'^\s*GEMINI_API_KEY\s*=\s*"?([^"\s]+)"?\s*$', line)
                if m:
                    key = m.group(1)
                    break
        except Exception as e:
            logging.warning(f"gemini_client: error leyendo {ENV_FILE_FALLBACK}: {e}")

    _cached_api_key = key
    return key


def diagnose(user_message: str, context: dict, knowledge: str) -> dict:
    """Consulta a Gemini vía la API REST. Devuelve None si no hay key o si falla."""
    api_key = _load_api_key()
    if not api_key:
        logging.warning("gemini_client: no hay GEMINI_API_KEY disponible.")
        return None

    system_instructions = (
        "Sos un ingeniero experto en optimización de laminado FDM para OrcaSlicer.\n"
        "Respondé ÚNICAMENTE un objeto JSON válido con exactamente esta estructura:\n"
        "{\n"
        '  "diagnosis_text": "<explicación concisa y técnica en español argentino/neutro>",\n'
        '  "proposed_changes": { "<clave_printconfig>": <valor_numerico_o_cadena> }\n'
        "}\n"
        "Las claves de proposed_changes deben ser nombres reales de PrintConfig de OrcaSlicer "
        "(ej: cool_plate_temp, hot_plate_temp, nozzle_temperature, filament_retraction_length, "
        "filament_retraction_speed, travel_speed, brim_width, brim_type, fan_min_speed, "
        "fan_max_speed, close_fan_the_first_x_layers, sparse_infill_density, layer_height, "
        "top_surface_speed, top_shell_layers, top_surface_pattern). Si no corresponde ningún "
        "ajuste concreto, dejá proposed_changes vacío ({}).\n"
        "Prestá especial atención a 'Valores que el usuario ya modificó respecto al preset "
        "original': si alguno está fuera de rango razonable para el material/impresora, "
        "señalalo explícitamente en diagnosis_text aunque el usuario no haya preguntado por eso.\n"
        "IMPORTANTE - tu alcance real: solo podés diagnosticar y proponer valores de "
        "parámetros del preset de proceso/filamento/impresora YA seleccionado (vía "
        "proposed_changes). NO podés cambiar qué impresora, filamento o perfil está "
        "seleccionado, ni abrir archivos, laminar, exportar G-code, ni ninguna otra acción "
        "de la interfaz. Si el mensaje del usuario pide algo de eso, NO lo ignores ni "
        "respondas con un diagnóstico genérico: en diagnosis_text aclará que no podés hacer "
        "esa acción vos y decile en una frase corta dónde hacerlo manualmente (ej. el "
        "selector de impresora/filamento/perfil en la barra lateral izquierda), y dejá "
        "proposed_changes vacío ({}).\n"
        "IMPORTANTE - límites físicos de la máquina: en 'Límites físicos reales de la "
        "máquina/filamento' vas a recibir machine_max_speed_x/y/z/e (mm/s reales de ESTA "
        "impresora) y filament_max_volumetric_speed. NUNCA propongas una velocidad por "
        "encima de esos límites reales, aunque te parezca típica para otra impresora.\n"
        "Si la consulta es una pregunta informativa que ya está resuelta en 'Estadísticas' "
        "(precio, tiempo estimado, peso de filamento), respondé con esos datos directamente "
        "en diagnosis_text y dejá proposed_changes vacío ({})."
    )

    prompt = (
        f"{system_instructions}\n\n"
        f"--- BASE DE CONOCIMIENTO RELEVANTE ---\n{knowledge}\n\n"
        f"--- CONTEXTO DE LAMINADO ---\n"
        f"Consulta del usuario: {user_message}\n"
        f"Impresora: {context.get('printer_preset', 'N/A')}\n"
        f"Filamento: {context.get('filament_preset', 'N/A')}\n"
        f"Perfil proceso: {context.get('process_preset', 'N/A')}\n"
        f"Valores que el usuario ya modificó respecto al preset original: "
        f"{json.dumps(context.get('config_diff_from_system', {}))}\n"
        f"Límites físicos reales de la máquina/filamento: "
        f"{json.dumps(context.get('machine_speed_limits', {}))}\n"
        f"Warnings de validación: {json.dumps(context.get('validation_warnings', []))}\n"
        f"Estadísticas: {json.dumps(context.get('print_statistics', {}))}\n"
    )

    body = json.dumps({
        "contents": [{"parts": [{"text": prompt}]}],
        "generationConfig": {
            "response_mime_type": "application/json",
            "thinkingConfig": {"thinkingBudget": 0},
            "maxOutputTokens": 800,
        },
    }).encode("utf-8")

    req = urllib.request.Request(
        f"{API_URL}?key={api_key}",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    # Presupuesto de tiempo total (no intentos de duración fija): el cliente
    # C++ (AICopilotPanel.cpp) corta la conexión a los 40s (timeout_max(40)),
    # así que cada intento usa el tiempo que realmente queda del presupuesto
    # en vez de un timeout fijo — evita tanto exceder el límite del C++ como
    # cortar prematuramente un intento que solo necesitaba unos segundos más.
    TOTAL_BUDGET_SECONDS = 33
    deadline = time.monotonic() + TOTAL_BUDGET_SECONDS
    last_error = None
    attempt = 0

    while True:
        attempt += 1
        remaining = deadline - time.monotonic()
        if remaining < 3:
            break
        try:
            logging.info(f"gemini_client: consultando {MODEL} (intento {attempt}, {remaining:.0f}s restantes)...")
            with urllib.request.urlopen(req, timeout=remaining) as resp:
                raw = json.loads(resp.read().decode("utf-8"))
            text = raw["candidates"][0]["content"]["parts"][0]["text"]

            # response_mime_type=json garantiza JSON válido, pero por las dudas
            # extraemos solo el primer objeto (raw_decode ignora basura final).
            decoder = json.JSONDecoder()
            data, _ = decoder.raw_decode(text.strip())

            data["confidence"] = "cloud"
            data["requires_confirmation"] = True
            if "proposed_changes" not in data or not isinstance(data["proposed_changes"], dict):
                data["proposed_changes"] = {}
            return data
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, KeyError, IndexError) as e:
            last_error = e

    logging.warning(f"gemini_client: error consultando la API tras reintentar: {last_error}")
    return None
