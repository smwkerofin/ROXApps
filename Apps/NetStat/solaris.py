# $Id: solaris.py,v 1.2 2004/03/17 18:39:46 stephen Exp $

"""netstat implementation for Solaris 8"""

import os
import sys
import string
import time

def stat():
    try:
        netstat=os.popen('netstat -i', 'r')
    except:
        return None
    res={}
    for line in netstat.readlines():
        if line[:4]=='Name' or line[0]==' ':
            continue
        words=string.split(line)
        if len(words)>6:
            iname=words[0]
            rpkts=long(words[4])
            tpkts=long(words[6])
            #print iname, rpkts, tpkts

            res[iname]=(rpkts, tpkts, -1, -1)
    netstat.close()
    if len(res.keys())<1:
        return None
    return res

def sockets_inet4(servers):
    try:
        if servers:
            netstat=os.popen('netstat -f inet -a', 'r')
        else:
            netstat=os.popen('netstat -f inet', 'r')
    except:
        print _('oops')
        return None

    socks=[]
    udp=0
    type='?'
    for line in netstat.readlines():
        if line=='':
            print 'EOF?'
            return socks
        l=line.strip()
        if l[:4]=='UDP:':
            udp=1
            type='UDP'
        elif l[:4]=='TCP:':
            udp=0
            type='TCP'
        elif l[:4]!='----':
            try:
                if udp:
                    try:
                        local, remote, state=line.split()
                    except ValueError:
                        local, state=line.split()
                        remote=''
                    sendq=''
                    recvq=''
                else:
                    local, remote, swind, sendq, rwind, recvq, state=line.split()
                socks.append((type, local, remote, sendq, recvq, state))
            except:
                pass
                
    return socks

def sockets_inet6(servers):
    pass

def sockets(servers=0):
    i4=sockets_inet4(servers)
    i6=sockets_inet6(servers)
    if i6 is None:
        return i4
    if i4 is None:
        return i6
    return i4+i6

def test():
    print stat()
    print '---'
    print sockets(0)
    print '---'
    print sockets(1)

if __name__=='__main__':
    test()
    
