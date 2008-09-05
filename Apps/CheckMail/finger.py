import os
import sys
import socket
from socket import gaierror, timeout
import time

PORT=79

def finger(target, port=PORT):
    user, host=target.split('@')
    s=socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    #addr=socket.gethostbyname(host)
    #print addr
    s.connect((host, port))
    s.send('/W %s\r\n' % user)
    return s

def get_line(sock):
    ans=''
    while True:
        c=sock.recv(1)
        if not c:
            if ans:
                return ans
            return None
        ans+=c
        if c=='\n':
            break
    return ans

class Finger(socket.socket):
    def __init__(self, target, port=PORT, long_output=False, timeout=None):
        self._port=port
        socket.socket.__init__(self, socket.AF_INET, socket.SOCK_STREAM)
        self.settimeout(timeout)
        user, host=target.split('@')
        self._user=user
        self._host=host
        #print 'connect to', host, port, time.time(), long_output, timeout
        self.connect((host, port))
        #print 'send', user, time.time()
        if long_output:
            self.send('/W %s\r\n' % user)
        else:
            self.send('%s\r\n' % user)

        self._ch=None
        #print 'ready to read', time.time()

    def _nextc(self):
        if self._ch is None:
            return self.recv(1)
        tmp=self._ch
        self._ch=None
        return tmp

    def peek(self):
        if self._ch is None:
            self._ch=self.recv(1)
        return self._ch
        
    def readline(self):
        ans=''
        while True:
            c=self._nextc()
            #print 'recv', ord(c)
            if not c:
                if ans:
                    return ans
                return None
            ans+=c
            if c=='\n':
                break
        return ans

    def readlines(self):
        while True:
            l=self.readline()
            if l is None:
                break
            yield l

    def __repr__(self):
        return '<Finger %s@%s:%d>' % (self._user, self._host, self._port)

def _test():
    #f=Finger('status@gate')
    #print f
    #while True:
    #    l=f.readline()
    #    if l is None:
    #        break
    #    print l[:-1]

    f2=Finger('kerofin@post', long_output=True, timeout=5)
    for l in f2.readlines():
        print l[:-1]

if __name__=='__main__':
    _test()
