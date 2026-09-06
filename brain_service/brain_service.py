#!/usr/bin/env python3
"""
brain_service.py - Servicio local de IA para OrcaSlicer (IAslicer Copilot).
Implementado exclusivamente con la biblioteca estándar de Python (sin dependencias externas).
"""

import sys
import json
import logging
from http.server import HTTPServer, BaseHTTPRequestHandler
from routing import route_query

HOST = "127.0.0.1"
PORT = 8787

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)]
)

class BrainRequestHandler(BaseHTTPRequestHandler):
    server_version = "IAslicerBrain/1.0"

    def _send_json_response(self, status_code: int, data: dict):
        response_bytes = json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(response_bytes)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(response_bytes)

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        if self.path in ("/", "/health"):
            self._send_json_response(200, {"status": "ok", "service": "IAslicer Brain Service"})
        else:
            self._send_json_response(404, {"error": "Not Found", "path": self.path})

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length) if content_length > 0 else b"{}"

        try:
            payload = json.loads(post_data.decode("utf-8")) if post_data else {}
        except json.JSONDecodeError:
            self._send_json_response(400, {"error": "Invalid JSON format"})
            return

        if self.path == "/echo":
            user_message = payload.get("user_message", "")
            logging.info(f"[/echo] Recibido mensaje: '{user_message}'")
            reply = f"Echo: {user_message}"
            self._send_json_response(200, {
                "status": "ok",
                "user_message": user_message,
                "reply": reply
            })
            return

        elif self.path == "/diagnose":
            user_message = payload.get("user_message", "")
            logging.info(f"[/diagnose] Consulta recibida: '{user_message}'")
            result = route_query(user_message, payload)
            # Asegurar contrato
            response_data = {
                "status": "ok",
                "diagnosis_text": result.get("diagnosis_text", ""),
                "proposed_changes": result.get("proposed_changes", {}),
                "confidence": result.get("confidence", "local"),
                "requires_confirmation": result.get("requires_confirmation", True)
            }
            self._send_json_response(200, response_data)
            return

        else:
            self._send_json_response(404, {"error": "Endpoint not found", "path": self.path})

    def log_message(self, format, *args):
        logging.info("%s - - [%s] %s" % (self.client_address[0], self.log_date_time_string(), format % args))


def run_server(host=HOST, port=PORT):
    server = HTTPServer((host, port), BrainRequestHandler)
    logging.info(f"Iniciando Brain Service en http://{host}:{port}/")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info("Deteniendo Brain Service...")
    finally:
        server.server_close()


if __name__ == "__main__":
    port = PORT
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    run_server(port=port)
