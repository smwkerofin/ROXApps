from gtk import *
#from newmailer import NewMailer
from mailers import Mailer

TARGET_URI_LIST=0
targets=[('text/uri-list', 0, TARGET_URI_LIST)]
text_uri_list=atom_intern("text/uri-list")

def set_mailer(widget, sm):
    mailer=widget.get_data('mailer')
    
    sm.current=mailer

    sm.update_win()

class SelectMailer(GtkDialog):
    def __init__(self, mailers, defmailer=None):
        if defmailer==None:
            defmailer=mailers[0]
        self.current=defmailer
        self.inform=None
        self.mailers=mailers
        self.updating=0
        self.list_size=0
            
        GtkDialog.__init__(self)
        self.set_title('Select mail program')

        self.vbox.set_spacing(4)
        hbox=GtkHBox(spacing=4)
        self.vbox.pack_start(hbox)

        label=GtkLabel('Mailers')
        hbox.pack_start(label, FALSE)

        self.mailer_menu=GtkOptionMenu()
        hbox.pack_start(self.mailer_menu, FALSE)

        menu=GtkMenu()
        self.mailer_menu.set_menu(menu)

        mw=0
        mh=0
        set=0
        i=0
        for mailer in mailers:
            if mailer==defmailer:
                set=i
            item=GtkMenuItem(mailer.name)
            item.set_data('mailer', mailer)
            item.show()
            size=item.size_request()
            # print size
            if size[0]>mw:
                mw=size[0]
            if size[1]>mh:
                mh=size[1]
            item.connect("activate", set_mailer, self)
            menu.append(item)
            i=i+1
            self.list_size+=1

        self.mailer_menu.set_usize(mw+50, mh+4)

        table=GtkTable(6, 2)
        self.vbox.pack_start(table)

        label=GtkLabel('Name')
        table.attach(label, 0, 1, 0, 1, xoptions=FILL, yoptions=FILL,
                     xpadding=2, ypadding=2)

        self.name=GtkEntry()
        self.name.set_text(defmailer.name)
        #self.name.set_editable(FALSE)
        table.attach(self.name, 1, 2, 0, 1, xpadding=2, ypadding=2)
        
        def name_change(widget, data):
            sm=data

            sm.current.name=widget.get_text()
            sm.update_win()

        self.name.connect('changed', name_change, self)
        
        label=GtkLabel('Location')
        table.attach(label, 0, 1, 1, 2, xoptions=FILL, yoptions=FILL,
                     xpadding=2, ypadding=2)

        self.loc=GtkEntry()
        self.loc.set_text(defmailer.loc)
        #self.loc.set_editable(FALSE)
        table.attach(self.loc, 1, 2, 1, 2, xpadding=2, ypadding=2)

        def loc_change(widget, data):
            sm=data

            sm.current.loc=widget.get_text()
            sm.update_win()

        self.loc.connect('changed', loc_change, self)
        
        self.loc.drag_dest_set(DEST_DEFAULT_ALL, targets,
                               GDK.ACTION_COPY | GDK.ACTION_PRIVATE)
        
        def drag_drop(widget, context, x, y, time, data=None):
            widget.drag_get_data(context, text_uri_list, time)
            return TRUE

        def drag_data_received(widget, context, x, y, sel_data, info, time, data):
            if sel_data.data==None:
                widget.drag_finish(context, FALSE, FALSE, time)
                return

            sm=data
            if info==TARGET_URI_LIST:
                sm.got_uri_list(widget, context, sel_data, time)
            else:
                widget.drag_finish(context, FALSE, FALSE, time)

        self.loc.connect("drag_data_received", drag_data_received, self)
        
        label=GtkLabel('Read command')
        table.attach(label, 0, 1, 2, 3, xoptions=FILL, yoptions=FILL,
                     xpadding=2, ypadding=2)

        self.read=GtkEntry()
        self.read.set_text(defmailer.read)
        #self.read.set_editable(FALSE)
        table.attach(self.read, 1, 2, 2, 3, xpadding=2, ypadding=2)

        def read_change(widget, data):
            sm=data

            sm.current.read=widget.get_text()
            sm.update_win()

        self.read.connect('changed', read_change, self)

        self.read_expl=GtkLabel()
        self.read_expl.set_text(defmailer.read_command())
        table.attach(self.read_expl, 1, 2, 3, 4, xpadding=2, ypadding=2)
       
        label=GtkLabel('Send command')
        table.attach(label, 0, 1, 4, 5, xoptions=FILL, yoptions=FILL,
                     xpadding=2, ypadding=2)

        self.send=GtkEntry()
        self.send.set_text(defmailer.send)
        #self.send.set_editable(FALSE)
        table.attach(self.send, 1, 2, 4, 5, xpadding=2, ypadding=2)

        def send_change(widget, data):
            sm=data

            sm.current.send=widget.get_text()
            sm.update_win()

        self.send.connect('changed', send_change, self)

        self.send_expl=GtkLabel()
        self.send_expl.set_text(defmailer.send_command('root@localhost',
                                                      'file_to_send',
                                                'This is the file you wanted'))
        table.attach(self.send_expl, 1, 2, 5, 6, xpadding=2, ypadding=2)
       
        self.vbox.show_all()
        
        self.mailer_menu.set_history(set)

        def set(widget, sm):
            if sm.inform!=None:
                sm.inform(sm, sm.current)
            else:
                sm.hide()
                
        def cancel(widget, sm):
            if sm.inform!=None:
                sm.inform(sm, None)
            else:
                sm.hide()
                #sm.destroy()

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

        button=GtkButton("Cancel")
        button.connect('clicked', cancel, self)
        hbox.pack_end(button, expand=FALSE)

        button=GtkButton("Set")
        button.connect('clicked', set, self)
        hbox.pack_end(button, expand=FALSE)
        
        #button=GtkButton("Edit mailer")
        #button.connect('clicked', edit, self)
        #hbox.pack_end(button, expand=FALSE)
        
        button=GtkButton("New mailer")
        button.connect('clicked', newm, self)
        hbox.pack_end(button, expand=FALSE)
        
        hbox.show_all()
        
    def set_callback(self, fn):
        self.inform=fn

    def add_mailer(self, mailer):
        item=GtkMenuItem(mailer.name)
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
        self.name.set_text(self.current.name)
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

    
    def got_uri_list(self, widget, context, sel_data, time):
        uris=extract_uris(sel_data.data)
        if not uris:
            widget.drag_finish(context, FALSE, FALSE, time)
            return
        for uri in uris:
            path=get_local_path(uri)
            if path:
                self.loc.set_text(path)
                self.update_win()
                widget.drag_finish(context, TRUE, FALSE, time)
                return

    def update_current(self):
        menu=self.mailer_menu.get_menu()
        item=menu.get_active()

        item.set_data('mailer', self.current)
        try:
            item.child.set_text(self.current.name)
        except:
            pass
        
        
