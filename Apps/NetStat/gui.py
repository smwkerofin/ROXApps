# $Id: gui.py,v 1.1 2002/10/19 14:35:21 stephen Exp $

import os
import sys
import string

import findrox

import rox.choices

from rox import g

import netstat

if len(sys.argv)>2 and sys.argv[1]=='-a':
    xid=int(sys.argv[2])
else:
    xid=None

#def mainquit():

stats=netstat.NetStat()

# Defaults
iface='ppp0'
#iface='lo'
fontname=None
levels=(500, 50, 1, 0)

# Load config
fname=rox.choices.load('NetStat', 'config')
try:
    inf=open(fname, 'r')

    lines=inf.readlines()
    for line in lines:
        line=string.strip(line)
        if len(line)<1 or line[0]=='#':
            continue
        eq=string.find(line, '=')
        if eq<1:
            continue
        if line[:eq]=='interface':
            iface=line[eq+1:]
    inf.close()
    
except:
    pass

if xid is None:
    win=g.Window()
else:
    win=g.Plug(xid)

win.connect('destroy', g.mainquit)

# Choose a nice small size for our applet...
win.set_size_request(48, 48)
win.set_border_width(4)

style=win.get_style()
#print style
#print dir(style)
#font=style.get_font()
#print style.base_gc, dir(style.base_gc)
#print style.base_gc()

vbox=g.VBox()
win.add(vbox)

# Our drawing canvas
can=g.DrawingArea()
vbox.pack_start(can)

ifdisp=g.Label(iface)
vbox.pack_start(ifdisp, g.FALSE, g.FALSE)

# Menu
menu=g.Menu()

item=g.MenuItem("Info")
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

item = g.MenuItem("Quit")
item.connect("activate", g.mainquit)
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
can.add_events(g.gdk.BUTTON_PRESS_MASK)
can.realize()

#print dir(can)
cmap=can.get_colormap()
#print dir(cmap)
blue=cmap.alloc_color('#0000ff')
green=cmap.alloc_color('#00bb00')
red=cmap.alloc_color('#ff0000')
high=cmap.alloc_color('#00ff00')
medium=green
low=cmap.alloc_color('#007700')
off=cmap.alloc_color('#000000')

colours=(high, medium, low, off)

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
    gc=widget.get_style().bg_gc[g.STATE_NORMAL]
    #print dir(widget.window)
    widget.window.draw_rectangle(gc, 1, 0, 0, width, height)
    #gc=widget.get_style().black_gc
    #print gc, dir(gc), gc.foreground
    #widget.draw_string(font, gc, 4, 24, iface)
    act=stats.getCurrent(iface)
    gc=widget.get_style().bg_gc[g.STATE_NORMAL]

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
        draw_arrow(widget.window, gc, pts, act[0])
        # Transmit
        cx=3*width/4
        left=width/2+2
        right=width-2
        pts=((cx-2,bot), (cx-2,mid), (left,mid), (cx,top), (right,mid),
             (cx+2,mid), (cx+2,bot))
        draw_arrow(widget.window, gc, pts, act[1])
    
can.connect('expose_event', expose)

def update():
    stats.update()
    can.queue_draw()
    return 1

tag=g.timeout_add(1000, update)

win.show_all()
g.mainloop()
