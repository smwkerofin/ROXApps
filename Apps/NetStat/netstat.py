# $Id$

"""Interface to network statistics, under Linux.  The function stat() returns
the data."""

import os
import sys
# import select
import string
import time

# cmd="netstat -i"
# cmd='date'

def stat():
    """Returns a dict containing the data, indexed by the interface name.
    Each entry is a tuple containg the received packet count and the
    transmitted packet count"""
    res={}
    handle=open('/proc/net/dev', 'r')
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
            rpkts=int(words[1])
            tbytes=int(words[8])
            tpkts=int(words[9])
        else:
            iname=words[0]
            rbytes=words[1]
            rpkts=int(words[2])
            tbytes=int(words[9])
            tpkts=int(words[10])
        res[iname]=(rpkts, tpkts)
            
    handle.close()
    if len(res.keys())<1:
        return None
    return res

class IfNetStat:
    def __init__(self, name, rpkt, tpkt):
        self.name=name
        self.prev=(rpkt, tpkt)
        self.current=(0, 0)

    def update(self, rpkt, tpkt):
        self.current=(rpkt-self.prev[0], tpkt-self.prev[1])
        self.prev=(rpkt, tpkt)

    def clear(self):
        self.current=None

    def getCurrent(self):
        return self.current
    def getTotal(self):
        return self.prev

class NetStat:
    def __init__(self):
        self.ifs={}
        self.update()

    def update(self):
        res=stat()
        if res is None:
            return
        for iface in self.ifs.keys():
            self.ifs[iface].clear()
        ifs=res.keys()
        for iface in ifs:
            try:
                self.ifs[iface].update(res[iface][0], res[iface][1])
            except KeyError:
                self.ifs[iface]=IfNetStat(iface, res[iface][0], res[iface][1])

    def getCurrent(self, iface):
        try:
            stat=self.ifs[iface]
        except KeyError:
            return None
        return stat.getCurrent()
    
def test(iface='lo'):
    stats=NetStat()
    while 1:
        stats.update()
        (rpkt, tpkt)=stats.getCurrent(iface)
        # print res
        print '%4s: %8d  %8d' % (iface, rpkt, tpkt)
        time.sleep(2)

if __name__=='__main__':
    if len(sys.argv)>1:
        test(sys.argv[1])
    else:
        test()
