import rox.loading
from rox import g
#from newmailer import NewMailer
from mailers import Mailer
from my_support import *

def set_mailer(widget, sm):
    mailer=widget.get_data('mailer')
    
    sm.current=mailer

    sm.update_win()

class SelectMailer(g.Dialog, rox.loading.XDSLoader):
    def __init__(self, mailers, defmailer=None):
        g.Dialog.__init__(self)
        rox.loading.XDSLoader.__init__(self, None)
        if defmailer==None:
            defmailer=mailers[0]
        self.current=defmailer
        self.inform=None
        self.inform_args=None
        self.mailers=mailers
        self.updating=0
        self.list_size=0
            
        g.Dialog.__init__(self)
        self.set_title('Select mail program')

        self.vbox.set_spacing(4)
        hbox=g.HBox(spacing=4)
        self.vbox.pack_start(hbox)

        label=g.Label('Mailers')
        hbox.pack_start(label, g.FALSE)

        self.mailer_menu=g.OptionMenu()
        hbox.pack_start(self.mailer_menu, g.FALSE)

        menu=g.Menu()
        self.mailer_menu.set_menu(menu)

        mw=0
        mh=0
        set=0
        i=0
        for mailer in mailers:
            if mailer==defmailer:
                set=i
            item=g.MenuItem(mailer.name)
            item.set_data('mailer', mailer)
            item.show()
            size=item.size_request()
            # print size
            if size[0]>mw:
                mw=size[0]
            if size[1]>mh:
                mh=size[1]
            item.connect("activate", self.set_mailer)
            menu.append(item)
            i=i+1
            self.list_size+=1

        #self.mailer_menu.set_usize(mw+50, mh+4)

        table=g.Table(6, 2)
        self.vbox.pack_start(table)

        label=g.Label('Name')
        table.attach(label, 0, 1, 0, 1, xoptions=g.FILL, yoptions=g.FILL,
                     xpadding=2, ypadding=2)

        self.name_ent=g.Entry()
        self.name_ent.set_text(defmailer.name)
        #self.name_ent.set_editable(g.FALSE)
        table.attach(self.name_ent, 1, 2, 0, 1, xpadding=2, ypadding=2)
        
        def name_change(widget, data):
            sm=data

            sm.current.name=widget.get_text()
            sm.update_win()

        self.name_ent.connect('changed', name_change, self)
        
        label=g.Label('Location')
        table.attach(label, 0, 1, 1, 2, xoptions=g.FILL, yoptions=g.FILL,
                     xpadding=2, ypadding=2)

        self.loc=g.Entry()
        self.loc.set_text(defmailer.loc)
        #self.loc.set_editable(g.FALSE)
        table.attach(self.loc, 1, 2, 1, 2, xpadding=2, ypadding=2)

        self.xds_proxy_for(self.loc)

        def loc_change(widget, data):
            sm=data

            sm.current.loc=widget.get_text()
            sm.update_win()

        self.loc.connect('changed', loc_change, self)
        
        label=g.Label('Read command')
        table.attach(label, 0, 1, 2, 3, xoptions=g.FILL, yoptions=g.FILL,
                     xpadding=2, ypadding=2)

        self.read=g.Entry()
        self.read.set_text(defmailer.read)
        #self.read.set_editable(g.FALSE)
        table.attach(self.read, 1, 2, 2, 3, xpadding=2, ypadding=2)

        def read_change(widget, data):
            sm=data

            sm.current.read=widget.get_text()
            sm.update_win()

        self.read.connect('changed', read_change, self)

        self.read_expl=g.Label('')
        self.read_expl.set_text(defmailer.read_command())
        table.attach(self.read_expl, 1, 2, 3, 4, xpadding=2, ypadding=2)
       
        label=g.Label('Send command')
        table.attach(label, 0, 1, 4, 5, xoptions=g.FILL, yoptions=g.FILL,
                     xpadding=2, ypadding=2)

        self.send=g.Entry()
        self.send.set_text(defmailer.send)
        #self.send.set_editable(g.FALSE)
        table.attach(self.send, 1, 2, 4, 5, xpadding=2, ypadding=2)

        def send_change(widget, data):
            sm=data

            sm.current.send=widget.get_text()
            sm.update_win()

        self.send.connect('changed', send_change, self)

        self.send_expl=g.Label('')
        self.send_expl.set_text(defmailer.send_command('root@localhost',
                                                      'file_to_send',
                                                'This is the file you wanted'))
        table.attach(self.send_expl, 1, 2, 5, 6, xpadding=2, ypadding=2)
       
        self.vbox.show_all()
        
        self.mailer_menu.set_history(set)

        def set(widget, sm):
            if sm.inform!=None:
                sm.inform(sm, sm.current, sm.inform_args)
            sm.hide()
                
        def cancel(widget, sm):
            if sm.inform!=None:
                sm.inform(sm, None, sm.inform_args)
            sm.hide()

        def newm(widget, sm):
            sm.update_current()
            mailer=Mailer('mailer', '/usr/bin/mailx', read='xterm -e %s')
            sm.add_mailer(mailer)
            sm.update_win()
                
        #def edit(widget, sm):
        #    # print "entered edit function"
        #    nm=NewMailer(sm.current)
        #    mailer=nm.process()
        #    if mailer!=None:
        #        sm.update_mailer(mailer)
                
        hbox=self.action_area

        button=g.Button("Cancel")
        button.connect('clicked', cancel, self)
        hbox.pack_end(button, expand=g.FALSE)

        button=g.Button("Set")
        button.connect('clicked', set, self)
        hbox.pack_end(button, expand=g.FALSE)
        
        #button=g.Button("Edit mailer")
        #button.connect('clicked', edit, self)
        #hbox.pack_end(button, expand=FALSE)
        
        button=g.Button("New mailer")
        button.connect('clicked', newm, self)
        hbox.pack_end(button, expand=g.FALSE)
        
        hbox.show_all()

    def set_mailer(self, widget, *args):
        mailer=widget.get_data('mailer')

        self.current=mailer

        self.update_win()

    def xds_load_from_file(self, path):
        self.loc.set_text(path)
        self.update_win()
        
    def set_callback(self, fn, args=None):
        self.inform=fn
        self.inform_args=args

    def add_mailer(self, mailer):
        item=g.MenuItem(mailer.name)
        item.set_data('mailer', mailer)
        item.show()
        item.connect("activate", set_mailer, self)
        self.mailer_menu.get_menu().append(item)
        self.mailers.append(mailer)
        self.list_size+=1
        self.mailer_menu.set_history(self.list_size-1)
        self.current=mailer
        self.update_win()
        
    def update_mailer(self, mailer):
        for m in mailers:
            if m.name==mailer.name:
                m.loc=mailer.loc
                m.read=mailer.read
                m.send=mailer.send
                break
        else:
            self.add_mailer(mailer)
        
    def update_win(self):
        if self.updating>0:
            return
        self.updating=1
        self.name_ent.set_text(self.current.name)
        self.loc.set_text(self.current.loc)
        if self.current.read is None:
            self.read.set_text("%s")
        else:
            self.read.set_text(self.current.read)
        self.send.set_text(self.current.send)

        self.send_expl.set_text(self.current.send_command('root@localhost',
                                                      'file_to_send',
                                                'This is the file you wanted'))
        self.read_expl.set_text(self.current.read_command())
        self.updating=0

    
    def update_current(self):
        menu=self.mailer_menu.get_menu()
        item=menu.get_active()

        item.set_data('mailer', self.current)
        try:
            item.child.set_text(self.current.name)
        except:
            pass
        
        
