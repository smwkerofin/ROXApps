# $Id: netstat.py,v 1.6 2003/08/23 17:00:54 stephen Exp $

"""Interface to network statistics, under Linux.  The function stat() returns
the data."""

import os
import sys
import string
import time

#print sys.platform
if sys.platform[:5]=='linux':
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
    def __init__(self, name, rpkt, tpkt, rbyte=-1, tbyte=-1):
        self.name=name
        self.prev=(rpkt, tpkt, rbyte, tbyte)
        self.clear()

    def update(self, rpkt, tpkt, rbyte=-1, tbyte=-1):
        #print 'update'
        #print self.name, rpkt, tpkt, rbyte, tbyte
        self.current[0]=rpkt-self.prev[0]
        self.current[1]=tpkt-self.prev[1]
        if rbyte>0:
            self.current[2]=rbyte-self.prev[2]
        else:
            self.current[2]=-1
        if tbyte>0:
            self.current[3]=tbyte-self.prev[3]
        else:
            self.current[3]=-1
        self.prev=(rpkt, tpkt, rbyte, tbyte)

    def clear(self):
        self.current=[0, 0, -1, -1]

    def getCurrent(self):
        #print 'getCurrent', self.prev, self.current
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
                self.ifs[iface].update(res[iface][0], res[iface][1],
                                       res[iface][2], res[iface][3])
            except KeyError:
                self.ifs[iface]=IfNetStat(iface, res[iface][0], res[iface][1],
                                          res[iface][2], res[iface][3])

    def getCurrent(self, iface):
        #print 'getCurrent', self.ifs, iface
        try:
            #print self.ifs
            stat=self.ifs[iface]
        except KeyError:
            return None
        #print stat
        return stat.getCurrent()
    
    def getSockets(self, servers=0):
        return sockets(servers)
    
def test(iface='lo'):
    stats=NetStat()
    while 1:
        stats.update()
        res=stats.getCurrent(iface)
        if res:
            (rpkt, tpkt, rbytes, tbytes)=res
            print '%4s: %8d %8d  %10d %10d' % (iface, rpkt, tpkt, rbytes, tbytes)
        else:
            print '%4s: not configured' % iface
        time.sleep(2)

if __name__=='__main__':
    if len(sys.argv)>1:
        test(sys.argv[1])
    else:
        test()
