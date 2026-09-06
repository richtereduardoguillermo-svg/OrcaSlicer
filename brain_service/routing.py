"""
routing.py - Enrutamiento de consultas entre modelo local y Gemini (CLI agy).
"""

def route_query(user_message: str, context: dict = None) -> dict:
    """
    Determina si la consulta se responde localmente o requiere Gemini.
    (En Fase 2 retorna eco simple).
    """
    return {
        "reply": f"Echo: {user_message}",
        "source": "echo_stub"
    }
