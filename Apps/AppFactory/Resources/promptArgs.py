#!/usr/bin/env python

# $Id: promptArgs.py,v 1.1 2001/11/05 15:55:24 stephen Exp $
#

import sys

icmd=""
prompt='Enter parameters'

if len(sys.argv)>1:
    i=1
    while i<len(sys.argv):
        if sys.argv[i]=="-p":
            if i+1<len(sys.argv):
                prompt=sys.argv[i+1]
                i=i+1
        else:
            icmd=sys.argv[i]

        i=i+1
        
def gtk1(g):
    win=g.GtkDialog()
    win.set_title('Enter parameters')

    vbox=win.vbox

    hbox=g.GtkHBox()
    vbox.pack_start(hbox)

    label=g.GtkLabel(prompt)
    hbox.pack_start(label)

    cmd=g.GtkEntry()
    cmd.set_text(icmd)
    hbox.pack_start(cmd)

    hbox=win.action_area

    def cancel(but, data):
        g.mainquit()

    but=g.GtkButton('Run without arguments')
    but.connect('clicked', cancel, None)
    hbox.pack_end(but)

    def run(but, data):
        print cmd.get_text()
        g.mainquit()

    but=g.GtkButton('Run with these arguments')
    hbox.pack_end(but)
    but.connect('clicked', run, None)

    win.show_all()
    win.connect("destroy", g.mainquit)

    g.mainloop()

def gtk2(g):
    win=g.Dialog()
    win.set_title('Enter parameters')

    vbox=win.vbox

    hbox=g.HBox()
    vbox.pack_start(hbox)

    label=g.Label(prompt)
    hbox.pack_start(label)

    cmd=g.Entry()
    cmd.set_text(icmd)
    hbox.pack_start(cmd)

    hbox=win.action_area

    def cancel(but, data):
        g.mainquit()

    but=g.Button('Run without arguments')
    but.connect('clicked', cancel, None)
    hbox.pack_end(but)

    def run(but, data):
        print cmd.get_text()
        g.mainquit()

    but=g.Button('Run with these arguments')
    hbox.pack_end(but)
    but.connect('clicked', run, None)

    win.show_all()
    win.connect("destroy", g.mainquit)

    g.mainloop()

try:
    import pygtk; pygtk.require('2.0')
    import gtk

    ver=gtk2
except:
    try:
        import gtk
        ver=gtk1
    except:
        sys.exit(0)

ver(gtk)

