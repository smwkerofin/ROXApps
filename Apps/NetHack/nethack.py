# $Id: nethack.py,v 1.2 2001/08/22 11:00:06 stephen Exp $
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

from setup import roles, races

class NHSelProfile(GtkWindow):
    def __init__(self):
        GtkWindow.__init__(self)

        self.set_title("Nethack")

        vbox=GtkVBox(FALSE, 4)
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

        hbox=GtkHBox(FALSE, 2)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=GtkLabel("Name")
        hbox.pack_start(label, FALSE, FALSE, 0)

        name_entry=GtkEntry()
        #name_entry.set_text(os.getlogin())
        name_entry.set_text(os.environ["LOGNAME"])
        name_entry.set_sensitive(FALSE)
        hbox.pack_start(name_entry, TRUE, TRUE, 0)
        self.name_entry=name_entry

        def name_used_toggled(button, obj):
            obj.name_entry.set_sensitive(button.get_active())
            
        name_used=GtkCheckButton("Use this name")
        name_used.set_active(FALSE)
        name_used.connect("toggled", name_used_toggled, self)
        hbox.pack_start(name_used, FALSE, FALSE, 0)
        self.name_used=name_used

        hbox=GtkHBox(FALSE, 2)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=GtkLabel("Role")
        hbox.pack_start(label, FALSE, FALSE, 0)

        role_menu=GtkOptionMenu()
        hbox.pack_start(role_menu, TRUE, TRUE, 0)

        menu=GtkMenu()
        role_menu.set_menu(menu)

        def select_role(item, obj):
            role=item.get_data('role')
            obj.role=role
        mw=0
        mh=0
        for role in roles:
            item=GtkMenuItem(role[0])
            item.set_data('role', role)
            item.show()
            size=item.size_request()
            # print size
            if size[0]>mw:
                mw=size[0]
            if size[1]>mh:
                mh=size[1]
            item.connect("activate", select_role, self)
            menu.append(item)

        role_menu.set_usize(mw+50, mh+4)
        role_menu.set_history(0)
        self.role_menu=role_menu

        hbox=GtkHBox(FALSE, 2)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=GtkLabel("Race")
        hbox.pack_start(label, FALSE, FALSE, 0)

        race_menu=GtkOptionMenu()
        hbox.pack_start(race_menu, TRUE, TRUE, 0)

        menu=GtkMenu()
        race_menu.set_menu(menu)

        def select_race(item, obj):
            race=item.get_data('race')
            obj.race=race
        mw=0
        mh=0
        for race in races:
            item=GtkMenuItem(race[0])
            item.set_data('race', race)
            item.show()
            size=item.size_request()
            # print size
            if size[0]>mw:
                mw=size[0]
            if size[1]>mh:
                mh=size[1]
            item.connect("activate", select_race, self)
            menu.append(item)

        race_menu.set_usize(mw+50, mh+4)
        race_menu.set_history(0)
        self.race_menu=race_menu

        hbox=GtkHBox(FALSE, 2)
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

        def edit_click(button, obj):
            obj.edit_current()
            
        edit=GtkButton("Edit profile")
        hbox.pack_start(edit, FALSE, FALSE, 0)
        edit.connect('clicked', edit_click, self)
        edit.set_sensitive(FALSE)
        self.edit=edit        

        def set_profile_active(button, row, col, event, obj):
            obj.remove.set_sensitive(TRUE)
            obj.edit.set_sensitive(TRUE)

        def set_profile_inactive(button, row, col, event, obj):
            obj.remove.set_sensitive(FALSE)
            obj.edit.set_sensitive(FALSE)

        self.profiles.connect('select_row', set_profile_active, self)
        self.profiles.connect('unselect_row', set_profile_inactive, self)

        vbox.show_all()

        self.load_profiles()

        def do_save(button, obj):
            obj.save_profiles()

        self.connect('destroy', do_save, self)

        self.role=roles[0]
        self.race=races[0]
        self.name=None

        self.busy_cursor=cursor_new(GDK.WATCH)

    def check_name(self):
        if self.name_used.get_active():
            self.name=self.name_entry.get_text()
        else:
            self.name=None
    
    def get_command(self):
        cmd="xterm -e /usr/games/nethack"

        self.check_name()
        if self.name:
            cmd=cmd+" -u "+self.name

        if self.role[1]:
            cmd=cmd+" -p "+self.role[1]

        if self.race[1]:
            cmd=cmd+" -r "+self.race[1]

        return cmd

    def start_game(self):
        #print "start game"

        self.set_sensitive(FALSE)
        # err, how do I set the curson?

        try:
            prof=self.get_profile()
            #print prof

            if prof:
                os.putenv("NETHACKOPTIONS", prof)

            cmd=self.get_command()
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

    def edit_current(self):
        #print "edit current"
        sel=self.profiles.selection
        name=self.profiles.get_text(sel[0], 0)
        #print "name=",name

        prog=rox.choices.load("MIME-types", "text/plain");
        if not prog:
            prog=rox.choices.load("MIME-types", "text");
        if not prog:
            rox.support.report_error("No defined run type for text?")
            return
        #print "prog=", prog
        
        def check_child(pid):
            res=os.waitpid(pid, os.WNOHANG)
            #print "checked ", pid, " got ", res

            if res[0]>0:
                return 0
            return 1

        pid=os.spawnv(os.P_NOWAIT, prog, (prog, name))
        timeout_add(1000, check_child, pid)
        #print "pid=", pid

    def get_profile(self):
        sel=self.profiles.selection
        if not sel:
            return None

        return self.profiles.get_text(sel[0], 0)

    def find_a_profile(self):
        try:
            default=os.environ["NETHACKOPTIONS"]
            if default[0]=='/':
                self.profiles.append([default])
            else:
                oname=rox.choices.save("NetHack", "rc")
                of=open(oname, "w")
                of.write("# Generated from $NETHACKOPTIONS\n")
                of.write(default)
                of.write("\n")
                of.close()
                self.profiles.append([oname])
                
        except:
            try:
                home=os.environ["HOME"]
                defname=home+"/.nethackrc"
                if os.access(defname, os.R_OK):
                    self.profiles.append([defname])
                else:
                    oname=rox.choices.save("NetHack", "rc")
                    of=open(oname, "w")
                    of.write("# No $NETHACKOPTIONS\n")
                    of.close()
                    self.profiles.append([oname])

            except:
                pass

    def load_profiles(self):
        self.profiles.freeze()
        self.profiles.clear()

        try:
            name=rox.choices.load("NetHack", "profiles")
            self.append_profiles_from(name)
        except:
            self.find_a_profile()

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
