#!/bin/bash
# Lanza el LLM local, el brain_service (si no están corriendo) y luego el
# binario empaquetado de IAslicer.
set -u

REPO_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
BRAIN_LOG="${REPO_DIR}/brain_service/brain_service.log"

LLAMA_SERVER_BIN="/home/eduardo/modelos/llama.cpp/build-vulkan/bin/llama-server"
LLAMA_MODEL="/home/eduardo/modelos/iaslicer-cerebro/Qwen3-4B-Instruct-2507-Q4_K_M.gguf"
LLAMA_LOG="/home/eduardo/modelos/iaslicer-cerebro/llama-server.log"

# Precalentar el LLM local (Qwen3-4B/Vulkan) para que la primera consulta del
# copiloto no pague el costo de arranque del modelo. Si falta el binario o el
# modelo (ej. en otra máquina), se omite: routing.py cae a Gemini Cloud solo.
if [ -x "${LLAMA_SERVER_BIN}" ] && [ -f "${LLAMA_MODEL}" ]; then
    if ! curl -s -o /dev/null -m 1 "http://127.0.0.1:8788/health"; then
        (nohup "${LLAMA_SERVER_BIN}" -m "${LLAMA_MODEL}" --host 127.0.0.1 --port 8788 \
            -ngl 99 -c 8192 >> "${LLAMA_LOG}" 2>&1 &)
    fi
fi

if ! curl -s -o /dev/null -m 1 "http://127.0.0.1:8787/health"; then
    (cd "${REPO_DIR}/brain_service" && nohup python3 brain_service.py >> "${BRAIN_LOG}" 2>&1 &)
    for _ in $(seq 1 20); do
        curl -s -o /dev/null -m 1 "http://127.0.0.1:8787/health" && break
        sleep 0.2
    done
fi

exec "${REPO_DIR}/build/package/orca-slicer" "$@"
