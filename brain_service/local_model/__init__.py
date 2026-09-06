"""
local_model - Cliente para el LLM local (Qwen3-4B-Instruct-2507 vía llama-server/Vulkan).

Arranca (si hace falta) un llama-server apuntando al modelo local en la iGPU
(Vulkan) y lo consulta por HTTP. Reemplaza el escalamiento a Gemini Cloud
para la mayoría de las consultas: mismo criterio de "propone, nunca aplica"
que el resto del brain_service, pero con respuesta en ~15-20s en vez de los
11-35s (o timeout) de 'agy'.

Sigue la misma regla del proyecto: solo stdlib de Python (subprocess +
urllib), sin dependencias pip externas.
"""

import json
import logging
import re
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

LLAMA_SERVER_BIN = "/home/eduardo/modelos/llama.cpp/build-vulkan/bin/llama-server"
MODEL_PATH = "/home/eduardo/modelos/iaslicer-cerebro/Qwen3-4B-Instruct-2507-Q4_K_M.gguf"
HOST = "127.0.0.1"
PORT = 8788
HEALTH_URL = f"http://{HOST}:{PORT}/health"
CHAT_URL = f"http://{HOST}:{PORT}/v1/chat/completions"

_server_process = None


def _is_server_up(timeout: float = 1.0) -> bool:
    try:
        with urllib.request.urlopen(HEALTH_URL, timeout=timeout) as resp:
            return resp.status == 200
    except Exception:
        return False


def ensure_server_started(wait_seconds: float = 25.0) -> bool:
    """Arranca llama-server si no está corriendo. Devuelve True si quedó disponible."""
    global _server_process

    if _is_server_up():
        return True

    if not Path(LLAMA_SERVER_BIN).exists() or not Path(MODEL_PATH).exists():
        logging.warning(
            f"local_model: binario o modelo no encontrado ({LLAMA_SERVER_BIN} / {MODEL_PATH}); "
            "no se puede arrancar el LLM local."
        )
        return False

    logging.info("local_model: arrancando llama-server (Qwen3-4B, Vulkan)...")
    _server_process = subprocess.Popen(
        [
            LLAMA_SERVER_BIN,
            "-m", MODEL_PATH,
            "--host", HOST,
            "--port", str(PORT),
            "-ngl", "99",
            "-c", "8192",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )

    deadline = time.time() + wait_seconds
    while time.time() < deadline:
        if _is_server_up():
            logging.info("local_model: llama-server listo.")
            return True
        time.sleep(0.3)

    logging.warning("local_model: llama-server no respondió a tiempo tras arrancarlo.")
    return False


def diagnose(user_message: str, context: dict, knowledge: str) -> dict:
    """Consulta al LLM local. Devuelve None si no está disponible o falla."""
    if not ensure_server_started():
        return None

    system_instructions = (
        "Sos un ingeniero experto en optimización de laminado FDM para OrcaSlicer.\n"
        "Respondé ÚNICAMENTE un objeto JSON válido (sin formato markdown ni bloques ```json).\n"
        "El JSON debe tener exactamente esta estructura:\n"
        "{\n"
        '  "diagnosis_text": "<explicación concisa y técnica en español argentino/neutro>",\n'
        '  "proposed_changes": { "<clave_printconfig>": <valor_numerico_o_cadena> }\n'
        "}\n"
        "Las claves de proposed_changes deben ser nombres reales de PrintConfig de OrcaSlicer, "
        "por ejemplo: cool_plate_temp, hot_plate_temp, nozzle_temperature, "
        "filament_retraction_length, filament_retraction_speed, travel_speed, brim_width, "
        "brim_type, fan_min_speed, fan_max_speed, close_fan_the_first_x_layers, "
        "sparse_infill_density, layer_height. No inventes nombres de claves distintos a esos "
        "estilos; si no corresponde ningún ajuste concreto, dejá proposed_changes vacío ({}).\n"
        "Prestá especial atención a 'Valores que el usuario ya modificó respecto al preset "
        "original': si alguno está fuera de rango razonable para el material/impresora "
        "(demasiado alto, demasiado bajo, o inconsistente con el resto), señalalo "
        "explícitamente en diagnosis_text aunque el usuario no haya preguntado por eso."
    )

    prompt = (
        f"--- BASE DE CONOCIMIENTO RELEVANTE ---\n{knowledge}\n\n"
        f"--- CONTEXTO DE LAMINADO ---\n"
        f"Consulta del usuario: {user_message}\n"
        f"Impresora: {context.get('printer_preset', 'N/A')}\n"
        f"Filamento: {context.get('filament_preset', 'N/A')}\n"
        f"Perfil proceso: {context.get('process_preset', 'N/A')}\n"
        f"Valores que el usuario ya modificó respecto al preset original: "
        f"{json.dumps(context.get('config_diff_from_system', {}))}\n"
        f"Warnings de validación: {json.dumps(context.get('validation_warnings', []))}\n"
        f"Estadísticas: {json.dumps(context.get('print_statistics', {}))}\n"
    )

    body = json.dumps({
        "messages": [
            {"role": "system", "content": system_instructions},
            {"role": "user", "content": prompt},
        ],
        "temperature": 0.3,
        "max_tokens": 400,
    }).encode("utf-8")

    req = urllib.request.Request(
        CHAT_URL, data=body, headers={"Content-Type": "application/json"}, method="POST"
    )

    try:
        logging.info("local_model: consultando Qwen3-4B local...")
        with urllib.request.urlopen(req, timeout=45) as resp:
            raw = json.loads(resp.read().decode("utf-8"))
        content = raw["choices"][0]["message"]["content"]

        cleaned = re.sub(r"^```(?:json)?\s*", "", content.strip(), flags=re.MULTILINE)
        cleaned = re.sub(r"```\s*$", "", cleaned, flags=re.MULTILINE).strip()
        match = re.search(r"\{.*\}", cleaned, re.DOTALL)
        if not match:
            logging.warning("local_model: la respuesta no contenía un JSON reconocible.")
            return None

        data = json.loads(match.group(0))
        data["confidence"] = "local"
        data["requires_confirmation"] = True
        if "proposed_changes" not in data:
            data["proposed_changes"] = {}
        return data
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, KeyError, IndexError) as e:
        logging.warning(f"local_model: error consultando el LLM local: {e}")
        return None
