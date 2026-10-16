import socket

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('0.0.0.0', 8888))
s.listen(5)
print('TCP server listening on 8888...')
c,a = s.accept()
print('Connected from', a)
while True:
    d = c.recv(1024)
    print('Received:', d)
    c.sendall(b'Recive required')
    
