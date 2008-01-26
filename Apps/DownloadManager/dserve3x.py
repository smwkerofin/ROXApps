import sys, time, os

import dbus, dbus.service , dbus.glib

#import findrox; findrox.version(2, 0, 2)
import rox, rox.options

import gobject

# DBus setup
service_name='uk.co.demon.kerofin.DownloadManager'
interface_name=service_name+'.Control'
new_interface_name=service_name+'.Control2'
object_path='/DownloadManager'

# Our options
import options

rox.app_options.notify()

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


class DownloadManager(dbus.service.Object):
    def __init__(self, bus, xid=None):
        #print bus
        bus_name=dbus.service.BusName(service_name, bus)
        dbus.service.Object.__init__(self, bus_name, object_path)

        self.clients={}
        self.active=[]
        self.current=None

        self.auto_show=False

        gobject.timeout_add(2000, self.update)

        rox.app_options.add_notify(self.opts_changed)

        #print self
        #print dir(self)
        #print self.Ping()

    def _message_cb(self, connection, message):
        #print '_message_cb', message
        self.current=message
        dbus.service.Object._message_cb(self, connection, message)
        self.current=None
        
    def update(self, *unused):
        #print 'update', unused
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

    @dbus.service.method(interface_name)
    def Ping(self):
        #print 'been pinged'
        return True

    @dbus.service.method(new_interface_name)
    def RequestID(self, agent):
        return str(self.current.get_sender())

    def can_start(self, id, server, fname):
        if id in self.clients:
            client=self.clients[id]
            client.update()
        else:
            client=Client(id, server, fname)
            self.clients[id]=client
            #print 'Added', id
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

    @dbus.service.method(interface_name)
    def CanIStart(self, server, fname):
        #print 'CanIStart', server, fname
        #print dir(self.current)
        id=self.current.get_sender()
        return self.can_start(id, server, fname)

    @dbus.service.method(new_interface_name)
    def CanIStartByID(self, id, server, fname):
        return self.can_start(id, server, fname)    

    @dbus.service.method(interface_name)
    def QueueSize(self):
        ntot=len(self.clients)
        nact=len(self.active)
        return ntot-nact
        
    @dbus.service.method(interface_name)
    def Update(self, size, total):
        id=self.current.get_sender()
        client=self.clients[id]
        #print 'Update', id, client
        client.update(size, total)

    @dbus.service.method(new_interface_name)
    def UpdateByID(self, id, size, total):
        client=self.clients[id]
        #print 'Update', id, client
        client.update(size, total)

    @dbus.service.method(interface_name)
    def Done(self):
        id=self.current.get_sender()
        client=self.clients[id]
        #print 'Done', id, client
        ##print client, 'has finished'
        self.lose_client(id)
        #print 'lost', id

    @dbus.service.method(new_interface_name)
    def DoneByID(self, id):
        client=self.clients[id]
        #print 'Done', id, client
        ##print client, 'has finished'
        self.lose_client(id)
        #print 'lost by id', id

    @dbus.service.method(interface_name)
    def Cancel(self, reason):
        #print 'in cancel', self, reason
        id=self.current.get_sender()
        #print id
        #client=self.clients[id]
        #print client
        #print 'about to call lose_client with', id
        self.lose_client(id)
        #print client, 'has been cancelled', reason

    @dbus.service.method(new_interface_name)
    def CancelByID(self, reason):
        #print 'in cancel by ID', self, reason
        #client=self.clients[id]
        #print client, 'has been cancelled', reason
        self.lose_client(id=id)

    @dbus.service.method(interface_name)
    def ShowOptions(self):
        self.opt_clicked()

    @dbus.service.method(interface_name)
    def GetStats(sel):
        act=[]
        for a in self.active:
            client=self.clients[a]
            act.append(str(client))
        
        #print 'returning', act
        return act

    @dbus.service.method(interface_name)
    def ShowWindow(self, show):
        self.user_show=show
        self.maybe_show()

    @dbus.service.method(interface_name)
    def GetWaiting(self):
        return len(self.clients)-len(self.active)

    @dbus.service.method(interface_name)
    def GetActive(self):
        return len(self.active)
    
    def lose_client(self, id=None, client=None):
        #print 'in lose_client', id, client
        if not id and not client:
            return
        
        if not id:
            id=client.id

        elif id not in self.clients:
            return
            
        print self.clients
        if id in self.active:
            self.active.remove(id)
        #print len(self.clients)
        del self.clients[id]
        #print 'removed client', id
        print len(self.clients), self.clients.keys()

        #print 'calling check_available'
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

    @dbus.service.signal(interface_name)
    def slot_available(self):
        #print 'raised slot available on', interface_name
        pass

    def check_available(self):
        #print 'in check_available', len(self.active), options.allow_nclient.int_value
        if len(self.active)<options.allow_nclient.int_value:
            self.slot_available()

    def __str__(self, *args):
        return 'DownloadManager-%s' % id(self)

