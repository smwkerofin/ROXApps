# $Id: netstat.py,v 1.4 2003/02/02 13:11:21 stephen Exp $

"""Interface to network statistics, under Linux.  The function stat() returns
the data."""

import os
import sys
import string
import time

if sys.platform=='linux' or sys.platform=='linux2':
    from linux import stat, sockets
elif sys.platform=='sunos5':
    from solaris import stat, sockets
else:
    sys.stderr.write('No implementation for platform %s\n' % sys.platform)
    def stat():
        return None
    def sockets(servers=0):
        return None

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
    
    def getSockets(self, servers=0):
        return sockets(servers)
    
def test(iface='lo'):
    stats=NetStat()
    while 1:
        stats.update()
        res=stats.getCurrent(iface)
        if res:
            (rpkt, tpkt)=res
            print '%4s: %8d  %8d' % (iface, rpkt, tpkt)
        else:
            print '%4s: not configured' % iface
        time.sleep(2)

if __name__=='__main__':
    if len(sys.argv)>1:
        test(sys.argv[1])
    else:
        test()
