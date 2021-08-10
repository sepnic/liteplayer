#!/usr/bin/env python
# -*- coding:utf-8 -*-
# Author: Qinglong <sysu.zqlong@gmail.com>

import socket, threading, sys
import datetime, time
import SimpleHTTPServer, BaseHTTPServer, SocketServer
from SocketServer import ThreadingMixIn

socket_port = 22808
http_port   = 12800

DOWNLOAD_FILE = "spk_test.mp3"
UPLOAD_FILE   = "record-16KHz-16bit-Mono.pcm"

UPLOAD_START = "GENIE_SOCKET_UPLOAD_START"
UPLOAD_END   = "GENIE_SOCKET_UPLOAD_END"

class StartSimpleHTTPServer:
    def __init__(self, port):
        self.port = port

    def start_server(self):
        Handler = SimpleHTTPServer.SimpleHTTPRequestHandler
        httpd = SocketServer.TCPServer(("", self.port), Handler)
        httpd.serve_forever()

def socket_callback_recv(conn):
    global UPLOAD_FILE
    while True:
        data = conn.recv(1024)
        if UPLOAD_START in data:
            now_time = datetime.datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
            UPLOAD_FILE = "record-" + now_time + ".pcm"
            print "-->GENIE_SOCKET_UPLOAD_START: %s" % UPLOAD_FILE
        elif UPLOAD_END in data:
            print "-->GENIE_SOCKET_UPLOAD_END: %s" % UPLOAD_FILE
            index = data.find(UPLOAD_END)
            if index > 0:
                with open(UPLOAD_FILE.decode('utf-8'), 'ab+') as f:
                    f.write(data[:index])
            break
        else:
            with open(UPLOAD_FILE.decode('utf-8'), 'ab+') as f:
                f.write(data)
    conn.close()

if __name__ == '__main__':
    http_server = StartSimpleHTTPServer(http_port)
    http_server_thread = threading.Thread(target=http_server.start_server)
    http_server_thread.setDaemon(True)
    http_server_thread.start()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(('0.0.0.0', socket_port))
    sock.listen(5)
    while True:
        conn, addr = sock.accept()
        thread_recv = threading.Thread(target=socket_callback_recv, args=(conn, ))
        thread_recv.setDaemon(True)
        thread_recv.start()
