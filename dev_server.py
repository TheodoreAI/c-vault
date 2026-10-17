#!/usr/bin/env python3
"""
C-Vault Local Development Server
Routes incoming HTTP requests to the compiled C CGI binary (cvault / cvault.exe).
Supports chunked streaming for multi-gigabyte uploads and HTTP 206 Range requests.
"""

import sys
import os
import socket
import urllib.parse
import subprocess
import shutil
from http.server import HTTPServer, BaseHTTPRequestHandler

PORT = 8080
BINARY_NAME = "cvault.exe" if sys.platform == "win32" else "cvault"
BINARY_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), BINARY_NAME)


def get_local_ip():
    """Finds the primary local LAN IP address accessible by other devices on Wi-Fi."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


class CGIRequestHandler(BaseHTTPRequestHandler):
    server_version = "CVaultDevServer/1.0"

    def do_GET(self):
        self.handle_cgi()

    def do_POST(self):
        self.handle_cgi()

    def do_HEAD(self):
        self.handle_cgi()

    def do_OPTIONS(self):
        self.handle_cgi()

    def do_DELETE(self):
        self.handle_cgi()

    def handle_cgi(self):
        self.close_connection = True
        if not os.path.exists(BINARY_PATH):
            self.send_error(
                500,
                f"Binary '{BINARY_NAME}' not found. Run 'make' first to compile the project.",
            )
            return

        parsed = urllib.parse.urlsplit(self.path)

        # Build CGI 1.1 environment
        env = os.environ.copy()
        env["GATEWAY_INTERFACE"] = "CGI/1.1"
        env["SERVER_PROTOCOL"] = self.request_version
        env["SERVER_SOFTWARE"] = self.server_version
        env["SERVER_NAME"] = "localhost"
        env["SERVER_PORT"] = str(PORT)
        env["REQUEST_METHOD"] = self.command
        env["REQUEST_URI"] = self.path
        env["PATH_INFO"] = urllib.parse.unquote(parsed.path)
        env["QUERY_STRING"] = parsed.query or ""
        env["REMOTE_ADDR"] = self.client_address[0]
        env["REMOTE_PORT"] = str(self.client_address[1])

        # Content headers
        content_length_str = self.headers.get("Content-Length")
        content_length = int(content_length_str) if content_length_str else 0
        if content_length > 0:
            env["CONTENT_LENGTH"] = str(content_length)

        content_type = self.headers.get("Content-Type")
        if content_type:
            env["CONTENT_TYPE"] = content_type

        # Forward HTTP headers
        for header, val in self.headers.items():
            norm_key = "HTTP_" + header.upper().replace("-", "_")
            env[norm_key] = val

        try:
            # Spawn the C binary
            proc = subprocess.Popen(
                [BINARY_PATH],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
            )

            # Stream request body from client directly to binary stdin in 64 KB chunks
            if content_length > 0 and self.rfile:
                bytes_left = content_length
                while bytes_left > 0:
                    chunk_size = min(65536, bytes_left)
                    chunk = self.rfile.read(chunk_size)
                    if not chunk:
                        break
                    proc.stdin.write(chunk)
                    bytes_left -= len(chunk)
            
            if proc.stdin:
                proc.stdin.close()

            # Parse CGI headers from binary stdout
            status_code = 200
            headers_list = []
            
            while True:
                line = proc.stdout.readline()
                if not line:
                    break
                decoded_line = line.decode("latin1").rstrip("\r\n")
                if not decoded_line:
                    # Blank line indicates end of CGI headers
                    break

                if ":" in decoded_line:
                    k, v = decoded_line.split(":", 1)
                    k = k.strip()
                    v = v.strip()
                    if k.lower() == "status":
                        try:
                            status_code = int(v.split()[0])
                        except ValueError:
                            status_code = 200
                    else:
                        headers_list.append((k, v))

            # Send HTTP response line
            self.send_response(status_code)
            for k, v in headers_list:
                self.send_header(k, v)
            self.end_headers()

            # Stream response body directly back to client in 64 KB chunks
            if self.command != "HEAD":
                while True:
                    chunk = proc.stdout.read(65536)
                    if not chunk:
                        break
                    self.wfile.write(chunk)
                self.wfile.flush()

            proc.wait()

            # Log errors if any occurred on stderr
            stderr_output = proc.stderr.read()
            if stderr_output:
                sys.stderr.write(
                    f"[{BINARY_NAME} stderr]: {stderr_output.decode('utf-8', errors='replace')}\n"
                )
                sys.stderr.flush()

        except Exception as e:
            import traceback
            traceback.print_exc()
            try:
                self.send_error(500, f"CGI execution error: {e}")
            except Exception:
                pass

    def log_message(self, format, *args):
        # Clean logging format: [IP] "METHOD PATH" CODE
        sys.stdout.write(
            f"[{self.client_address[0]}] {format % args}\n"
        )
        sys.stdout.flush()


def run():
    local_ip = get_local_ip()
    server_address = ("0.0.0.0", PORT)
    httpd = HTTPServer(server_address, CGIRequestHandler)

    print("=" * 60, flush=True)
    print("  C-Vault Local Development Server (CGI 1.1)", flush=True)
    print("=" * 60, flush=True)
    print(f"  Local Machine : http://127.0.0.1:{PORT}/", flush=True)
    print(f"  iPhone / Wi-Fi: http://{local_ip}:{PORT}/", flush=True)
    print("=" * 60, flush=True)
    print("  Press Ctrl+C to stop the server.", flush=True)
    print("", flush=True)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        httpd.server_close()


if __name__ == "__main__":
    run()
