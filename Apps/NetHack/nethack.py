# $Id$
#
# Shell for running nethack

import findrox
import rox.choices
import rox.MultipleChoice
import rox.support

from gtk import *
import sys
import os

import string

TARGET_URI_LIST=0
targets=[('text/uri-list', 0, TARGET_URI_LIST)]
text_uri_list=atom_intern("text/uri-list")

class NHSelProfile(GtkWindow):
    def __init__(self):
        GtkWindow.__init__(self)

        self.set_title("Nethack")

        vbox=GtkVBox(FALSE)
        self.add(vbox)

        label=GtkLabel("Profiles")
        vbox.pack_start(label, FALSE, FALSE, 0)

        scrw=GtkScrolledWindow()
        scrw.set_usize(320, 160)
        vbox.pack_start(scrw, TRUE, TRUE, 0)

        self.profiles=GtkCList()
        self.profiles.set_selection_mode(SELECTION_SINGLE)
        self.profiles.drag_dest_set(DEST_DEFAULT_ALL, targets,
                               GDK.ACTION_COPY | GDK.ACTION_PRIVATE)
        def drag_drop(widget, context, x, y, time, data=None):
            widget.drag_get_data(context, text_uri_list, time)
            return TRUE

        def drag_data_received(widget, context, x, y, sel_data, info, time, data):
            if sel_data.data==None:
                widget.drag_finish(context, FALSE, FALSE, time)
                return

            win=data
            if info==TARGET_URI_LIST:
                win.got_uri_list(widget, context, sel_data, time)
            else:
                widget.drag_finish(context, FALSE, FALSE, time)

        self.profiles.connect("drag_data_received", drag_data_received, self)
        
        
        scrw.add(self.profiles)

        hbox=GtkHBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        def start_click(button, obj):
            obj.start_game()

        start=GtkButton("Start game")
        hbox.pack_start(start, FALSE, FALSE, 0)
        start.connect('clicked', start_click, self)

        def remove_click(button, obj):
            obj.remove_current()

        remove=GtkButton("Remove profile")
        hbox.pack_start(remove, FALSE, FALSE, 0)
        remove.connect('clicked', remove_click, self)
        remove.set_sensitive(FALSE)
        self.remove=remove

        def set_profile_active(button, row, col, event, obj):
            obj.remove.set_sensitive(TRUE)

        def set_profile_inactive(button, row, col, event, obj):
            obj.remove.set_sensitive(FALSE)

        self.profiles.connect('select_row', set_profile_active, self)
        self.profiles.connect('unselect_row', set_profile_inactive, self)

        vbox.show_all()

        self.load_profiles()

        def do_save(button, obj):
            obj.save_profiles()

        self.connect('destroy', do_save, self)

    def start_game(self):
        #print "start game"

        self.set_sensitive(FALSE)

        try:
            prof=self.get_profile()
            #print prof

            if prof:
                os.putenv("NETHACKOPTIONS", prof)

            cmd="xterm -e /usr/games/nethack"
            #print cmd

            os.system(cmd)
            
        except:
            rox.support.report_exception()

        self.set_sensitive(TRUE)

    def remove_current(self):
        #print "remove current"
        sel=self.profiles.selection
        #print sel, sel[0]
        #name=self.profiles.get_text(sel[0], 0)
        #print name
        self.profiles.remove(sel[0])
        self.remove.set_sensitive(FALSE)

    def get_profile(self):
        sel=self.profiles.selection
        if not sel:
            return None

        return self.profiles.get_text(sel[0], 0)

    def load_profiles(self):
        self.profiles.freeze()
        self.profiles.clear()

        try:
            name=rox.choices.load("NetHack", "profiles")
            self.append_profiles_from(name)
        except:
            pass

        self.profiles.thaw()

    def append_profiles_from(self, name):
        file=open(name, "r")

        lines=file.readlines()
        #print lines

        for line in lines:
            name=string.strip(line)
            row=[name]
            #print line, name, row
            self.profiles.append(row)

        file.close()

    def got_uri_list(self, widget, context, sel_data, time):
        uris=string.split(sel_data.data, '\r\n')
        if not uris:
            widget.drag_finish(context, FALSE, FALSE, time)
            return
        self.profiles.freeze()
        for uri in uris:
            path=rox.support.get_local_path(uri)
            if path:
                self.profiles.append([path])
                                
        widget.drag_finish(context, TRUE, FALSE, time)
        self.profiles.thaw()

    def save_profiles(self):
        try:
            name=rox.choices.save("NetHack", "profiles")
            file=open(name, "w")
            n=self.profiles.rows
            #print n

            for i in range(n):
                name=self.profiles.get_text(i, 0)
                #print name
                file.write(name+"\n")

            file.close()
        except:
            pass

win=NHSelProfile()
win.connect('destroy', mainquit)
win.show()

mainloop()

#win.save_profiles()
