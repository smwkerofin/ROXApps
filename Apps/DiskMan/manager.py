import os
import sys

class Manager(object):
    def __init__(self):
        self.clients=[]

    def get_disks(self):
        pass

    def add_client(self, client):
        self.clients.append(client)

    def disk_event(self, disk, event, *args):
        #print 'disk_event', event
        for client in self.clients:
            if hasattr(client, event):
                #print 'send', event, 'to', client
                meth=getattr(client, event)
                #print meth
                fargs=(disk,)+args
                meth(*fargs)

