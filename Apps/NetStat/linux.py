# $Id: linux.py,v 1.1 2003/06/09 17:29:11 stephen Exp $

"""netstat implementation for Linux"""

import os
import sys
import string
import time
import socket

def stat():
    """Returns a dict containing the data, indexed by the interface name.
    Each entry is a tuple containg the received packet count and the
    transmitted packet count"""
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
        if len(w2)>1:
            iname=w2[0]
            rbytes=w2[1]
            rpkts=long(words[1])
            tbytes=long(words[8])
            tpkts=long(words[9])
        else:
            iname=words[0]
            rbytes=words[1]
            rpkts=long(words[2])
            tbytes=long(words[9])
            tpkts=long(words[10])
        res[iname]=(rpkts, tpkts)
            
    handle.close()
    if len(res.keys())<1:
        return None
    return res

states={1: 'Established', 2: 'SYN sent', 3: 'SYN received',
        4: 'FIN wait 1', 5: 'FIN wait 2', 6: 'Time wait',
        7: 'Close', 8: 'Close wait', 9: 'Last ACK',
        10: 'Listen', 11: 'Closing'
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

addrs={}

def fqdn(addr):
    if addr in addrs:
        return addrs[addr]
    try:
        #print addr
        #hostname, aliaslist, addrlist=socket.gethostbyaddr(addr)
        #print hostname
        fqdn=socket.getfqdn(addr)
        #print fqdn
    except:
        print sys.exc_info()[:2]
        return addr
    addrs[addr]=fqdn
    return fqdn

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
            pass
        
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
            pass
        
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


