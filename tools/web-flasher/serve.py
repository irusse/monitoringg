"""Local flasher page. Open http://127.0.0.1:8765/ after this script starts."""
import http.server
import os
import socketserver

PORT = 8765
os.chdir(os.path.dirname(os.path.abspath(__file__)))
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("127.0.0.1", PORT), http.server.SimpleHTTPRequestHandler) as httpd:
    print(f"http://127.0.0.1:{PORT}/")
    httpd.serve_forever()
