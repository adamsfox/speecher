#!/usr/bin/env python3
"""TLS file server for the Linux update E2E.

Serves the fixture directory (update-manifest.json and the NEW AppImage) over
HTTPS on 127.0.0.1. The updater requires an https download URL, and Qt 6.8
ignores SSL_CERT_FILE, so the runner bind-mounts a CA directory containing
this server's certificate over /etc/ssl/certs for the app process only.
"""

import http.server
import ssl
import sys


def main() -> None:
    directory, cert, key, port = (
        sys.argv[1],
        sys.argv[2],
        sys.argv[3],
        int(sys.argv[4]),
    )

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=directory, **kwargs)

    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    print(f"serving {directory} on https://127.0.0.1:{port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
