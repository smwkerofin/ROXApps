# $Id$

import os
import sys

import findrox

#from rox import g
import gtk
import GTK
import GDK

import netstat

if len(sys.argv)>2 and sys.argv[1]=='-a':
    xid=int(sys.argv[2])
else:
    xid=None

#def mainquit():

stats=netstat.NetStat()
iface='ppp0'
#iface='lo'
font=None

if xid is None:
    win=gtk.GtkWindow()
else:
    win=gtk.GtkPlug(xid)

win.connect('destroy', gtk.mainquit)

# Choose a nice small size for our applet...
win.set_usize(48, 48)
win.set_border_width(4)

style=win.get_style()
# print style
font=style.font
#print dir(style)
#print style.base_gc, dir(style.base_gc)
#print style.base_gc()

vbox=gtk.GtkVBox()
win.add(vbox)

# Our drawing canvas
can=gtk.GtkDrawingArea()
vbox.pack_start(can)

ifdisp=gtk.GtkLabel(iface)
vbox.pack_start(ifdisp, gtk.FALSE, gtk.FALSE)

# Menu
menu=gtk.GtkMenu()

item=gtk.GtkMenuItem("Info")
import InfoWin
iw=InfoWin.InfoWin('NetStat', 'Monitor network activity',
           '0.0.1 (4th Octobber 2002)',
           'Stephen Watson', 'http://www.kerofin.demon.co.uk/rox/')
iw.connect('delete_event', lambda iw: iw.hide())
def show_infowin(widget, data):
    iw=data
    iw.show()
item.connect("activate", show_infowin, iw)
#print iw
menu.append(item)

item = gtk.GtkMenuItem("Quit")
item.connect("activate", gtk.mainquit)
menu.append(item)

menu.show_all()

def click(widget, event, data=None):
    #print widget, event, data
    if event.button==1:
        pass
    elif event.button==3:
        menu.popup(None, None, None, event.button, event.time)
        return 1
    return 0

can.connect("button_press_event", click)
can.add_events(GDK.BUTTON_PRESS_MASK)
can.realize()

#cmap=can.colormap
blue=gtk.colour_alloc(can, 0x0000ff)
green=gtk.colour_alloc(can, 0x00bb00)
red=gtk.colour_alloc(can, 0xff0000)
high=gtk.colour_alloc(can, 0x00ff00)
medium=green
low=gtk.colour_alloc(can, 0x007700)
off=gtk.colour_alloc(can, 0)

colours=(high, medium, low, off)
levels=(500, 50, 1, 0)

def draw_arrow(drawable, gc, pts, act):
    tmp=gc.foreground
    col=red
    for i in range(len(levels)):
        if act>=levels[i]:
            col=colours[i]
            break
    gc.foreground=col
    drawable.draw_polygon(gc, 1, pts)
    gc.foreground=tmp

def expose(widget, event):
    (x, y, width, height)=widget.get_allocation()
    gc=widget.get_style().bg_gc[GTK.STATE_NORMAL]
    widget.draw_rectangle(gc, 1, 0, 0, width, height)
    #gc=widget.get_style().black_gc
    #print gc, dir(gc), gc.foreground
    #widget.draw_string(font, gc, 4, 24, iface)
    act=stats.getCurrent(iface)
    gc=widget.get_style().bg_gc[GTK.STATE_NORMAL]

    top=2
    bot=height-2
    mid=height/2
    if act and len(act)>1:
        # Receive
        cx=width/4
        left=2
        right=width/2-2
        pts=((cx-2,top), (cx-2,mid), (left,mid), (cx,bot), (right,mid),
             (cx+2,mid), (cx+2,top))
        draw_arrow(widget, gc, pts, act[0])
        # Transmit
        cx=3*width/4
        left=width/2+2
        right=width-2
        pts=((cx-2,bot), (cx-2,mid), (left,mid), (cx,top), (right,mid),
             (cx+2,mid), (cx+2,bot))
        draw_arrow(widget, gc, pts, act[1])
    
can.connect('expose_event', expose)

def update():
    stats.update()
    can.queue_draw()
    return 1

tag=gtk.timeout_add(1000, update)

win.show_all()
gtk.mainloop()
