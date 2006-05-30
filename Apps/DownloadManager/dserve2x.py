#! /usr/bin/env python

import dbus
import sys, time, os

#import findrox; findrox.version(2, 0, 2)
import rox, rox.options

import gobject

# DBus setup
service_name='uk.co.demon.kerofin.DownloadManager'
interface_name=service_name+'.Control'
object_path='/DownloadManager'

# Our options
import options

class Client:
    """Tracks a download client"""
    def __init__(self, id, server, fname):
        """Store data"""
        self.id=id
        self.server=server
        self.fname=fname
        
        self.last=time.time()
        self.start=self.last
        self.running=False

        self.size=0
        self.total=0

    def expired(self):
        """Has the client failed to update recently and therefore has expired?"""
        #print time.time(), self.last, expires
        return time.time()-self.last>options.client_expires.int_value

    def run(self):
        """Client has moved from pending to downloading"""
        self.running=True

    def update(self, size=None, total=None):
        """Client has downloaded another chunk"""
        self.last=time.time()
        if size is not None:
            self.size=size
        if total is not None:
            self.total=total

    def __str__(self):
        return '<Client id="%s" host="%s" file="%s" last=%f running=%s start=%f size=%d total=%d>' % (self.id, self.server, self.fname, self.last, self.running, self.start, self.size, self.total)

ID=0
SERVER=1
FNAME=2
STATE=3
SIZE=4
TOTAL=5
PER=6
AGE=7

class DownloadManager(dbus.Object):
    def __init__(self, bus, xid=None):
        service=dbus.Service(service_name, bus)
        dbus.Object.__init__(self, object_path, service, [self.Ping,
                                                          self.CanIStart,
                                                          self.QueueSize,
                                                          self.Update,
                                                          self.Done,
                                                          self.Cancel,
                                                          self.ShowOptions,
                                                          self.GetStats,
                                                          self.GetActive,
                                                          self.GetWaiting,
                                                          self.ShowWindow])

        self.clients={}
        self.active=[]

        self.auto_show=False

        gobject.timeout_add(2000, self.update)

        rox.app_options.add_notify(self.opts_changed)

    def update(self, *unused):
        nactive=0
        for act in self.active:
            nactive+=1

        keys=self.clients.keys()
        keys.sort()
        nwait=0
        for id in keys:
            if id not in self.active:
                nwait+=1

        if nactive>=options.show_nactive.int_value or nwait>=options.show_nwaiting.int_value:
            self.auto_show=True
        else:
            self.auto_show=False
        self.maybe_show()
                
        return True

    def maybe_show(self):
        pass

    def opts_changed(self):
        self.update()
        
    def Ping(self, method):
        return 1

    def CanIStart(self, method, server, fname):
        id=method.get_sender()
        if id in self.clients:
            client=self.clients[id]
            client.update()
        else:
            client=Client(id, server, fname)
            self.clients[id]=client
        #print 'Start request from', id
        #print 'active', self.active

        if len(self.active)>=options.allow_nclient.int_value:
            try:
                self.check_expired()
            except:
                print sys.exc_info()[:2]

        if len(self.active)<options.allow_nclient.int_value:
            #print 'starting', id
            self.active.append(id)
            client.run()
            return True

        #print 'Rejected, %d in queue' % (len(self.clients)-len(self.active))
        return False

    def QueueSize(self, method):
        ntot=len(self.clients)
        nact=len(self.active)
        return ntot-nact
        
    def Update(self, method, size, total):
        id=method.get_sender()
        client=self.clients[id]
        client.update(size, total)

    def Done(self, method):
        id=method.get_sender()
        client=self.clients[id]
        #print client, 'has finished'
        self.lose_client(id)

    def Cancel(self, method, reason):
        #print 'in cancel', self, method, reason
        id=method.get_sender()
        client=self.clients[id]
        #print client, 'has been cancelled', reason
        self.lose_client(id=id)

    def ShowOptions(self, method):
        rox.edit_options()

    def GetStats(self, method):
        act=[]
        for a in self.active:
            client=self.clients[a]
            act.append(str(client))
        
        #print 'returning', act
        return act

    def ShowWindow(self, method, show):
        self.user_show=show
        self.maybe_show()

    def GetWaiting(self, method):
        return len(self.clients)-len(self.active)

    def GetActive(self, method):
        return len(self.active)
    
    def lose_client(self, id=None, client=None):
        if not id and not client:
            return
        
        if not id:
            id=client.id
            
        if id in self.active:
            self.active.remove(id)
        del self.clients[id]

        self.check_available()
        #print 'checked for available slot'

    def check_expired(self):
        # print 'in check_expired', self.clients
        for c in self.clients:
            #print 'id', c
            client=self.clients[c]
            #print client
            if client.expired():
                #print client, 'has expired'
                self.lose_client(c)

    def slotAvailable(self):
        #print 'in slotAvailable'
        self.emit_signal(interface=interface_name,
                         signal_name='slot_available')
        #print 'emitted signal'
        #print rox._toplevel_windows
        #print rox._in_mainloops

    def check_available(self):
        #print 'in check_available', len(self.active), allow_nclient.int_value
        if len(self.active)<options.allow_nclient.int_value:
            self.slotAvailable()

    def __str__(self, *args):
        return 'DownloadManager-%s' % id(self)

