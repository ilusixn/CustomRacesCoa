"""Sert un dossier en local et enregistre ce que la page lui envoie (POST /save).

Sert à extraire les airs du prototype « Carnet du Barde » : on ouvre la page dans un navigateur, un petit
script (tools/export_airs.js) lit ses 40 airs et les envoie ici, sans rien recopier à la main.

Usage : python collect_server.py <dossier à servir> <fichier de sortie> [port]
Le serveur n'écoute que sur 127.0.0.1 et s'arrête après la première réception.
"""

import functools
import http.server
import sys
import threading
from pathlib import Path


def main():
    root, output = Path(sys.argv[1]), Path(sys.argv[2])
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 8765
    done = threading.Event()

    class Handler(http.server.SimpleHTTPRequestHandler):
        def do_POST(self):
            if self.path != "/save":
                self.send_error(404)
                return
            body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            output.write_bytes(body)
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"ok")
            print(f"{len(body)} octets enregistrés dans {output}", flush=True)
            done.set()

        def log_message(self, *args):
            pass

    handler = functools.partial(Handler, directory=str(root))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as server:
        threading.Thread(target=server.serve_forever, daemon=True).start()
        print(f"En écoute sur http://127.0.0.1:{port}/", flush=True)
        done.wait(timeout=600)


if __name__ == "__main__":
    main()
