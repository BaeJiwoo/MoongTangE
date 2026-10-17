import subprocess
import sys
import os
import signal

server = subprocess.Popen([sys.argv[1], '--port', '0', '--run-for', '8'],
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    line = server.stdout.readline().strip()
    if not line.startswith('Listening 127.0.0.1:'):
        raise RuntimeError(f'Unexpected server startup: {line}')
    subprocess.run([sys.argv[2], line.split(' ', 1)[1]], check=True, timeout=6)
    if server.wait(timeout=10) != 0:
        raise RuntimeError(server.stderr.read())
finally:
    if server.poll() is None:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()

# POSIX delivers SIGTERM to the application; Windows terminate() forcibly kills it.
if os.name != 'nt':
    server = subprocess.Popen([sys.argv[1], '--port', '0'],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        if not server.stdout.readline().startswith('Listening 127.0.0.1:'):
            raise RuntimeError('Signal test server did not start')
        server.send_signal(signal.SIGTERM)
        if server.wait(timeout=3) != 0:
            raise RuntimeError('SIGTERM did not cause a clean server shutdown')
    finally:
        if server.poll() is None:
            server.kill()
            server.wait()
