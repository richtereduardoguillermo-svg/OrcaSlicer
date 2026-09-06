#!/bin/bash
# Lanza el brain_service (si no está corriendo) y luego el binario empaquetado
# de IAslicer. Si este script fue quien lo arrancó, lo apaga de nuevo al
# cerrar el laminador (no si ya estaba corriendo).
set -u

REPO_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
BRAIN_LOG="${REPO_DIR}/brain_service/brain_service.log"

STARTED_BRAIN_PID=""

cleanup() {
    if [ -n "${STARTED_BRAIN_PID}" ]; then
        kill "${STARTED_BRAIN_PID}" 2>/dev/null
    fi
}
trap 'cleanup; exit 0' EXIT INT TERM

if ! curl -s -o /dev/null -m 1 "http://127.0.0.1:8787/health"; then
    (cd "${REPO_DIR}/brain_service" && exec nohup python3 brain_service.py >> "${BRAIN_LOG}" 2>&1) &
    STARTED_BRAIN_PID=$!
    for _ in $(seq 1 20); do
        curl -s -o /dev/null -m 1 "http://127.0.0.1:8787/health" && break
        sleep 0.2
    done
fi

"${REPO_DIR}/build/package/orca-slicer" "$@"
