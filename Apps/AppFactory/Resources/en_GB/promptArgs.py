#!/usr/bin/env python
#
# $Id: promptArgs.py,v 1.1 2001/09/12 13:52:02 stephen Exp $
#

import sys

from gtk import *

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
        

win=GtkDialog()
win.set_title('Enter parameters')

vbox=win.vbox

hbox=GtkHBox()
vbox.pack_start(hbox)

label=GtkLabel(prompt)
hbox.pack_start(label)

cmd=GtkEntry()
cmd.set_text(icmd)
hbox.pack_start(cmd)

hbox=win.action_area

def cancel(but, data):
    mainquit()

but=GtkButton('Run without arguments')
but.connect('clicked', cancel, None)
hbox.pack_end(but)

def run(but, data):
    print cmd.get_text()
    mainquit()

but=GtkButton('Run with these arguments')
hbox.pack_end(but)
but.connect('clicked', run, None)

win.show_all()
win.connect("destroy", mainquit)

mainloop()
