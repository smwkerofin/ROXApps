# $Id: linux.py,v 1.5 2006/04/29 11:18:19 stephen Exp $

"""netstat implementation for Linux"""

import os
import sys
import string
import time
import socket

import rox

def stat():
    """Returns a dict containing the data, indexed by the interface name.
    Each entry is a tuple containg the received packet count,
    transmitted packet count, received bytes and transmitted bytes"""
    res={}
    try:
        handle=open('/proc/net/dev', 'r')
    except:
        return None
    while 1:
        # print 'reading line from', handle
        line=string.strip(handle.readline())
        # print line
        if len(line)<1:
            break
        words=string.split(line)
        if len(words)<1:
            continue
        if words[0]=='Inter-|' or words[0]=='face':
            continue
        # print line
        w2=string.split(words[0], ':')
        #print words[0], w2
        iname=w2[0]
        if len(w2)>1 and w2[1]:
            #print w2
            rbytes=long(w2[1])
            rpkts=long(words[1])
            tbytes=long(words[8])
            tpkts=long(words[9])
        else:
            rbytes=long(words[1])
            rpkts=long(words[2])
            tbytes=long(words[9])
            tpkts=long(words[10])
        #print iname, rbytes
        res[iname]=(rpkts, tpkts, rbytes, tbytes)
            
    handle.close()
    if len(res.keys())<1:
        return None
    return res

def get_start_time():
    """Return the time from which the data returned by stat() is valid.
    This is most probably the boot time."""

    s=os.stat('/proc/net/dev')
    return s.st_mtime

states={1: _('Established'), 2: _('SYN sent'), 3: _('SYN received'),
        4: _('FIN wait 1'), 5: _('FIN wait 2'), 6: _('Time wait'),
        7: _('Close'), 8: _('Close wait'), 9: _('Last ACK'),
        10: _('Listen'), 11: _('Closing')
        }
def get_state(state):
    s=int(state, 16)
    if states.has_key(s):
        return states[s]
    return s

def dotquad(addr):
    a=addr & 0xff
    b=(addr>>8) & 0xff
    c=(addr>>16) & 0xff
    d=addr>>24
    return '%d.%d.%d.%d' % (a, b, c, d)

class AddrCache:
    def __init__(self):
        self.addrs={}
        self.q=[]
        self.tout=0

    def get(self, addr):
        #print 'get', addr, type(addr)
        if type(addr)==int:
            addr=dotquad(addr)

        #print self.addrs
        if addr in self.addrs:
            return self.addrs[addr]

        self.queue_lookup(addr)
        return addr

    def queue_lookup(self, addr):
        if addr in self.q:
            return
        
        self.q.insert(0, addr)
        #print len(self.q)

        if not self.tout:
            self.tout=rox.g.timeout_add(1000, self.lookup, addr)

    def lookup(self, ignored):
        if len(self.q)<1:
            return False

        addr=self.q.pop()
        while addr in self.addrs:
            if len(self.q)<1:
                return False
            addr=self.q.pop()
        
        #print self, addr
        
        try:
            fqdn=socket.getfqdn(addr)
        except:
            fqdn=addr

        self.addrs[addr]=fqdn

        #print fqdn, len(self.q)
        if len(self.q)>0:
            return True

        rox.g.timeout_remove(self.tout)
        return False


addrs=AddrCache()

def fqdn(addr):
    return addrs.get(addr)

def serv(port, proto):
    return port

def get_addr4(address, proto, islocal=0):
    #print address, proto, islocal
    addr, port=address.split(':')
    #print addr, port
    addr=long(addr, 16)
    #print addr
    if addr>0:
        addr=fqdn(dotquad(addr))
    else:
        addr='*'
    #print addr
    port=int(port, 16)
    #print port
    if port>0 and port<1024:
        try:
            port=serv(port, proto)
        except:
            pass
    elif port>1024:
        pass
    else:
        port='*'
    #print port
    #print addr, port, ':'.join((addr, str(port)))
    return ':'.join((addr, str(port)))

def get_addr6(address, proto):
    #print address, proto
    addr, port=address.split(':')
    #print addr, port
    iaddr=long(addr, 16)
    #print iaddr
    if iaddr>0:
        tmp=addr
        while tmp.startswith('00'):
            tmp=tmp[2:]
        addr=':'+tmp
        if addr.startswith(':FFFF0000') and len(addr)==17:
            addr=fqdn(dotquad(long(addr[9:], 16)))
        else:
            addr=fqdn(addr)
    else:
        addr='*'
    #print addr
    port=int(port, 16)
    #print port
    if port>0 and port<1024:
        try:
            port=serv(port, proto)
        except:
            pass
    elif port>1024:
        pass
    else:
        port='*'
    #print port
    #print addr, port, ':'.join((addr, str(port)))
    return ':'.join((addr, str(port)))

def is_bound(addr):
    host, port=addr.split(':')
    #print host, port
    return long(host, 16)!=0 and int(port, 16)!=0

def ip4_sockets(servers, socks, fname, type):
    try:
        f=file(fname, 'r')
    except:
        return

    header=f.readline()
    for line in f.readlines():
        l=line.strip()
        try:
            sl, local, remote, state, queue, trail=l.split(' ', 5)
            sendq, recvq=queue.split(':')
            sendq=int(sendq, 16)
            recvq=int(recvq, 16)
            #print sl, local, remote
            if not servers and not is_bound(local):
                continue
            #print servers
            local=get_addr4(local, type, 1)
            remote=get_addr4(remote, type, 0)
            #print local, remote
            state=get_state(state)
            socks.append((type, local, remote, sendq, recvq, state))
        except:
            print l
            print sys.exc_info()[:2]
            raise
        
def ip6_sockets(servers, socks, fname, type):
    try:
        f=file(fname, 'r')
    except:
        return

    header=f.readline()
    for line in f.readlines():
        l=line.strip()
        try:
            sl, local, remote, state, queue, trail=l.split(' ', 5)
            sendq, recvq=queue.split(':')
            sendq=int(sendq, 16)
            recvq=int(recvq, 16)
            #print sl, local, remote
            if not servers and not is_bound(local):
                continue
            #print servers
            local=get_addr6(local, type)
            remote=get_addr6(remote, type)
            #print local, remote
            state=get_state(state)
            socks.append((type, local, remote, sendq, recvq, state))
        except:
            #print l
            print sys.exc_info()[:2]
        
def tcp_sockets(servers, socks):
    ip4_sockets(servers, socks, '/proc/net/tcp', 'tcp')
        
def udp_sockets(servers, socks):
    ip4_sockets(servers, socks, '/proc/net/udp', 'udp')
    
def tcp6_sockets(servers, socks):
    ip6_sockets(servers, socks, '/proc/net/tcp6', 'tcp6')
        
def udp6_sockets(servers, socks):
    ip6_sockets(servers, socks, '/proc/net/udp6', 'udp6')
        
def sockets(servers=0):
    """Returns list of tuples with (type, local, remote, sendq, recvq, state)"""
    socks=[]

    tcp_sockets(servers, socks)
    tcp6_sockets(servers, socks)
    udp_sockets(servers, socks)
    udp6_sockets(servers, socks)

    return socks


