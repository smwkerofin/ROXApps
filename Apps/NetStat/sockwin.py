# $Id$
import os
import sys
import string

import findrox

import rox

TYPE=0
LOCAL=1
REMOTE=2
SENDQ=3
RECVQ=4
STATE=5

class SocketsWindow(rox.Window):
    titles=('Type', 'Local address', 'Remote', 'Send-Q', 'Recv-Q', 'State')

    def __init__(self, stats, servers=0):
        self.stats=stats
        self.show_servers=servers
        
        rox.Window.__init__(self)
        if servers:
            self.set_title('All Sockets')
        else:
            self.set_title('Active Sockets')
            
        vbox=rox.g.VBox()
        self.add(vbox)

        swin=rox.g.ScrolledWindow()
        swin.set_size_request(480, 320)
        swin.set_policy(rox.g.POLICY_NEVER, rox.g.POLICY_AUTOMATIC)
        vbox.pack_start(swin, rox.g.TRUE, rox.g.TRUE, 0)

        self.store=rox.g.ListStore(str, str, str, str, str, str)
        self.view=rox.g.TreeView(self.store)

        def str_col(view, icol, title):
            cell=rox.g.CellRendererText()
            col=rox.g.TreeViewColumn(title, cell, text=icol)
            col.set_sort_column_id(icol)
            view.append_column(col)

        for i in range(len(SocketsWindow.titles)):
            str_col(self.view, i, SocketsWindow.titles[i])

        sel=self.view.get_selection()
        sel.set_mode(rox.g.SELECTION_NONE)

        swin.add(self.view)

        self.load(stats.getSockets(servers))

        vbox.show_all()

        self.to=None
        self.connect('show', self.on_show)

    def __del__(self):
        if self.to:
            rox.g.timeout_remove(self.to)

    def on_show(self, ignored=None):
        self.to=rox.g.timeout_add(5*1000, self.update)

    def update(self):
        self.load(self.stats.getSockets(self.show_servers))
        return 1

    def load(self, socks):
        self.store.clear()
        if socks is None:
            return
        for sock in socks:
            iter=self.store.append()
            try:
                for i in range(len(SocketsWindow.titles)):
                    self.store.set(iter, i, sock[i])
            except:
                pass
