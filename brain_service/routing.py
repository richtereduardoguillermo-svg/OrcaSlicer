"""
routing.py - Motor de diagnóstico y enrutamiento (Local primero + Gemini CLI agy).
"""

import os
import re
import json
import logging
import subprocess
from pathlib import Path

import local_model

KNOWLEDGE_DIR = Path(__file__).resolve().parent / "knowledge"

KEYWORD_TOPICS = {
    "warping.md": ["warp", "alabe", "despeg", "levant", "esquina", "cama", "pega", "bed"],
    "stringing.md": ["string", "hilo", "telara", "pelo", "retra", "chorreo"],
    "layer_adhesion.md": ["adhes", "capa", "delamin", "separ", "fragil", "resistencia", "rompe"],
    "first_layer.md": ["primera capa", "primer capa", "first layer", "aplast", "nivel"]
}

def load_relevant_knowledge(user_message: str, validation_warnings: list = None) -> str:
    """Selecciona y combina fragmentos markdown relevantes según keywords."""
    msg_lower = (user_message or "").lower()
    selected_files = set()

    for filename, keywords in KEYWORD_TOPICS.items():
        if any(kw in msg_lower for kw in keywords):
            selected_files.add(filename)

    if validation_warnings:
        for w in validation_warnings:
            opt = (w.get("opt_key") or "").lower()
            if "bed" in opt or "temp" in opt:
                selected_files.add("warping.md")
            if "retract" in opt:
                selected_files.add("stringing.md")
            if "layer" in opt:
                selected_files.add("first_layer.md")

    docs = []
    for fname in selected_files:
        fpath = KNOWLEDGE_DIR / fname
        if fpath.exists():
            try:
                docs.append(fpath.read_text(encoding="utf-8"))
            except Exception as e:
                logging.warning(f"Error leyendo conocimiento {fname}: {e}")

    return "\n\n---\n\n".join(docs)

def diagnose_local(user_message: str, context: dict) -> dict:
    """
    Diagnóstico local basado en reglas expertas y base de conocimiento.
    Garantiza respuestas instantáneas y reproducibles para problemas típicos.
    """
    msg_lower = user_message.lower()
    filament = (context.get("filament_preset") or "").upper()
    user_msg_all = msg_lower + " " + filament.lower()

    # Detección de material
    is_petg = "petg" in user_msg_all
    is_abs = "abs" in user_msg_all or "asa" in user_msg_all
    is_tpu = "tpu" in user_msg_all
    is_pla = "pla" in user_msg_all or (not is_petg and not is_abs and not is_tpu)

    # Caso 1: Warping / Despegue
    if any(k in msg_lower for k in ["warp", "alabe", "despeg", "levant", "esquinas"]):
        if is_petg:
            return {
                "diagnosis_text": "El warping en PETG ocurre por enfriamiento prematuro y falta de área de contacto en la placa. Se recomienda elevar la temperatura de cama a 75°C, desactivar el ventilador durante las 4 primeras capas y activar borde exterior (brim).",
                "proposed_changes": {
                    "hot_plate_temp": 75,
                    "close_fan_the_first_x_layers": 4,
                    "brim_type": "outer_only",
                    "brim_width": 5.0
                },
                "confidence": "local",
                "requires_confirmation": True
            }
        elif is_abs:
            return {
                "diagnosis_text": "El ABS/ASA sufre una fuerte contracción térmica. Es indispensable mantener la cama a 100°C+, retrasar el encendido del ventilador de capa y usar borde exterior.",
                "proposed_changes": {
                    "hot_plate_temp": 100,
                    "close_fan_the_first_x_layers": 5,
                    "brim_type": "outer_only",
                    "brim_width": 8.0
                },
                "confidence": "local",
                "requires_confirmation": True
            }
        else: # PLA u otro
            return {
                "diagnosis_text": "Para evitar despegue en las esquinas, aumentá la temperatura de la placa a 45°C, retrasá el encendido del ventilador y agregá un brim de 5mm.",
                "proposed_changes": {
                    "cool_plate_temp": 45,
                    "close_fan_the_first_x_layers": 3,
                    "brim_type": "outer_only",
                    "brim_width": 5.0
                },
                "confidence": "local",
                "requires_confirmation": True
            }

    # Caso 2: Stringing / Hilos
    if any(k in msg_lower for k in ["string", "hilo", "telara", "pelos"]):
        return {
            "diagnosis_text": "Los hilos de material indican retracción insuficiente o velocidad inadecuada de traslados. Ajustamos retracción y aumentamos velocidad de traslados libres.",
            "proposed_changes": {
                "filament_retraction_length": 1.2,
                "filament_retraction_speed": 40.0,
                "travel_speed": 200
            },
            "confidence": "local",
            "requires_confirmation": True
        }

    # Caso 3: Delaminación / Capas débiles
    if any(k in msg_lower for k in ["adhes", "capa", "delamin", "separ", "fragil", "rompe"]):
        return {
            "diagnosis_text": "La separación entre capas se debe a un enfriamiento excesivo del flujo. Se recomienda moderar la velocidad del ventilador de capa.",
            "proposed_changes": {
                "fan_max_speed": 40,
                "fan_min_speed": 20
            },
            "confidence": "local",
            "requires_confirmation": True
        }

    # Caso 4: Warnings estructurados de validación
    warnings = context.get("validation_warnings", [])
    if warnings:
        w0 = warnings[0]
        opt = w0.get("opt_key", "")
        msg = w0.get("message", "")
        return {
            "diagnosis_text": f"Advertencia detectada en el laminado: {msg}. Se ajusta el parámetro correspondiente.",
            "proposed_changes": {opt: 0.2} if opt else {},
            "confidence": "local",
            "requires_confirmation": True
        }

    return None

def diagnose_cloud_gemini(user_message: str, context: dict, knowledge: str) -> dict:
    """Consulta a Gemini vía el CLI 'agy -p' con prompt estructurado para respuesta JSON."""
    system_instructions = (
        "Sos un ingeniero experto en optimización de laminado FDM para OrcaSlicer.\n"
        "Respondé ÚNICAMENTE un objeto JSON válido (sin formato markdown ni bloques ```json).\n"
        "El JSON debe tener exactamente esta estructura:\n"
        "{\n"
        '  "diagnosis_text": "<explicación concisa y técnica en español argentino/neutro>",\n'
        '  "proposed_changes": { "<clave_printconfig>": <valor_numerico_o_cadena> },\n'
        '  "confidence": "cloud",\n'
        '  "requires_confirmation": true\n'
        "}\n"
        "Las claves de proposed_changes deben ser nombres reales de PrintConfig (ej: cool_plate_temp, hot_plate_temp, nozzle_temperature, filament_retraction_length, brim_width, brim_type, fan_min_speed, close_fan_the_first_x_layers, sparse_infill_density, layer_height).\n"
        "Prestá especial atención a 'Valores que el usuario ya modificó respecto al preset original': si alguno está fuera de rango razonable para el material/impresora (demasiado alto, demasiado bajo, o inconsistente con el resto), señalalo explícitamente en diagnosis_text aunque el usuario no haya preguntado por eso.\n"
        "IMPORTANTE - tu alcance real: solo podés diagnosticar y proponer valores de parámetros del preset de proceso/filamento/impresora YA seleccionado (vía proposed_changes). NO podés cambiar qué impresora, filamento o perfil está seleccionado, ni abrir archivos, laminar, exportar G-code, ni ninguna otra acción de la interfaz. Si el mensaje del usuario pide algo de eso (ej. 'cambiá a la Ender 3', 'abrí este archivo', 'laminá la pieza'), NO lo ignores ni respondas con un diagnóstico genérico del preset actual: en diagnosis_text aclará que no podés hacer esa acción vos y decile en una frase corta dónde hacerlo manualmente (ej. el selector de impresora/filamento/perfil en la barra lateral izquierda), y dejá proposed_changes vacío ({})."
    )

    prompt = (
        f"{system_instructions}\n\n"
        f"--- BASE DE CONOCIMIENTO RELEVANTE ---\n{knowledge}\n\n"
        f"--- CONTEXTO DE LAMINADO ---\n"
        f"Consulta del usuario: {user_message}\n"
        f"Impresora: {context.get('printer_preset', 'N/A')}\n"
        f"Filamento: {context.get('filament_preset', 'N/A')}\n"
        f"Perfil proceso: {context.get('process_preset', 'N/A')}\n"
        f"Valores que el usuario ya modificó respecto al preset original: {json.dumps(context.get('config_diff_from_system', {}))}\n"
        f"Warnings de validación: {json.dumps(context.get('validation_warnings', []))}\n"
        f"Estadísticas: {json.dumps(context.get('print_statistics', {}))}\n"
    )

    try:
        logging.info("Invocando Gemini vía 'agy -p'...")
        cmd = ["agy", "-p", prompt]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=35)
        if result.returncode == 0 and result.stdout.strip():
            raw = result.stdout.strip()
            # Limpiar posibles bloques ```json ... ```
            cleaned = re.sub(r"^```(?:json)?\s*", "", raw, flags=re.MULTILINE)
            cleaned = re.sub(r"```\s*$", "", cleaned, flags=re.MULTILINE).strip()
            # Extraer el primer objeto {...}
            match = re.search(r"\{.*\}", cleaned, re.DOTALL)
            if match:
                data = json.loads(match.group(0))
                data["confidence"] = "cloud"
                data["requires_confirmation"] = True
                if "proposed_changes" not in data:
                    data["proposed_changes"] = {}
                return data
    except Exception as e:
        logging.warning(f"Error invocando agy: {e}")

    return None

def route_query(user_message: str, context: dict = None) -> dict:
    """Enruta la consulta: local primero; si no calza en reglas directas, usa Gemini."""
    context = context or {}
    knowledge = load_relevant_knowledge(user_message, context.get("validation_warnings"))

    # 1. Reglas locales instantáneas (casos típicos ya conocidos)
    local_res = diagnose_local(user_message, context)
    if local_res:
        logging.info("Diagnóstico resuelto por REGLAS LOCALES (instantáneo).")
        return local_res

    # 2. LLM local (Qwen3-4B vía llama-server/Vulkan) — reemplaza a Gemini en la
    #    mayoría de los casos: mismo criterio, sin depender de la red ni de agy.
    llm_res = local_model.diagnose(user_message, context, knowledge)
    if llm_res:
        logging.info("Diagnóstico resuelto por LLM LOCAL (Qwen3-4B).")
        return llm_res

    # 3. Gemini Cloud, solo si el LLM local no está disponible o falló
    cloud_res = diagnose_cloud_gemini(user_message, context, knowledge)
    if cloud_res:
        logging.info("Diagnóstico resuelto por GEMINI (Cloud) — fallback tras fallo del LLM local.")
        return cloud_res

    # 4. Fallback genérico
    return {
        "diagnosis_text": f"Recibí tu consulta: '{user_message}'. No se detectaron fallas críticas en los parámetros activos.",
        "proposed_changes": {},
        "confidence": "local",
        "requires_confirmation": False
    }
