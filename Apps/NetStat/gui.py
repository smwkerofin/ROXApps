# $Id: gui.py,v 1.10 2003/06/09 17:29:11 stephen Exp $

import os
import sys
import string

import findrox

import rox.choices

from rox import g
import pango

import netstat
import rox.Menu
import rox.applet
import rox.options

from sockwin import SocketsWindow

#import gc; gc.set_debug(gc.DEBUG_LEAK)

if len(sys.argv)>2 and sys.argv[1]=='-a':
    xid=long(sys.argv[2])
else:
    xid=None

app_dir=os.path.dirname(sys.argv[0])
version=''
from xml.dom.minidom import parse
doc=parse(os.path.join(app_dir, 'AppInfo.xml'))
about=doc.getElementsByTagName("About")
def get_text(node):
    t=''
    for sub in node.childNodes:
        if sub.nodeType==node.TEXT_NODE:
            t+=sub.data
    return t
for a in about:
    for node in a.childNodes:
        if node.nodeType==node.ELEMENT_NODE:
            if node.tagName=='Version':
                version=get_text(node)

stats=netstat.NetStat()

# Defaults
iface='ppp0'
select_cmd=''
adjust_cmd=''
levels=[500, 50, 1, 0]
wsize=48

pids=[]

# Load old style config if there is no new one
if rox.choices.load('NetStat', 'Options.xml') is None:
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
            elif line[:eq]=='connect':
                select_cmd=line[eq+1:]
        inf.close()
    
    except:
        pass

# Load config
rox.setup_app_options('NetStat')
interface=rox.options.Option('interface', iface)
connect=rox.options.Option('connect', select_cmd)
disconnect=rox.options.Option('disconnect', adjust_cmd)
medium_level=rox.options.Option('medium', levels[1])
high_level=rox.options.Option('high', levels[0])
win_size=rox.options.Option('wsize', wsize)

ifdisp=None
win=None

def options_changed():
    global iface, select_cmd
    #print 'in options_changed()'
    if interface.has_changed:
        #print 'iface=%s' % interface.value
        iface=interface.value
        if ifdisp:
            ifdisp.set_text(iface)
    if connect.has_changed:
        select_cmd=connect.value
    if disconnect.has_changed:
        adjust_cmd=disconnect.value
    if medium_level.has_changed:
        levels[1]=medium_level.int_value
    if high_level.has_changed:
        levels[0]=high_level.int_value
    if win_size.has_changed and win:
        win.set_size_request(win_size.int_value, win_size.int_value)

rox.app_options.add_notify(options_changed)

rox.app_options.notify()

if xid is None:
    win=rox.Window()
else:
    win=rox.applet.Applet(xid)

#win.connect('destroy', g.mainquit)

# Choose a nice small size for our applet...
win.set_size_request(wsize, wsize)
win.set_border_width(2)

style=win.get_style()
#print style
#print dir(style)
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
rox.Menu.set_save_name('NetStat')
menu=rox.Menu.Menu('main', [
    ('/Info', 'show_info', ''),
    ('/Active sockets...', 'show_active', ''),
    ('/All sockets...', 'show_all', ''),
    ('/Options...', 'edit_options', ''),
    ('/Quit', 'do_quit', '')
    ])

import InfoWin
iw=InfoWin.InfoWin('NetStat', 'Monitor network activity',
           version,
           'Stephen Watson', 'http://www.kerofin.demon.co.uk/rox/')
iw.connect('delete_event', lambda iw: iw.hide())

class MenuHelper:
    def show_info(unused):
        iw.show()
        #import rox.InfoWin
        #rox.InfoWin.infowin('NetStat')
    def do_quit(unused):
        rox.toplevel_unref()
    def edit_options(unused):
        rox.edit_options()
    def show_active(unused):
        w=SocketsWindow(stats)
        w.show()
    def show_all(unused):
        w=SocketsWindow(stats, 1)
        w.show()

menu_helper=MenuHelper()
menu.attach(win, menu_helper)

def reap():
    global pids
    active=[]
    for pid in pids:
        res=os.waitpid(pid, os.WNOHANG)
        if not res or res[0]!=pid:
            active.append(pid)
    pids=active
    if len(pids)==0:
        return 0
    return 1

def click(widget, event, data=None):
    #print widget, event, data
    if event.button==1:
        if select_cmd and len(select_cmd)>1:
            pid=os.spawnl(os.P_NOWAIT, '/bin/sh', 'sh', '-c', select_cmd)
            if pid>0:
                pids.append(pid)
                if len(pids)==1:
                    g.timeout_add(10000, reap)
            return 1
    elif event.button==2:
        if adjust_cmd and len(adjust_cmd)>1:
            pid=os.spawnl(os.P_NOWAIT, '/bin/sh', 'sh', '-c', adjust_cmd)
            if pid>0:
                pids.append(pid)
                if len(pids)==1:
                    g.timeout_add(10000, reap)
            return 1
    elif event.button==3:
        if xid:
            #menu.popup(menu_helper, event, win.position_menu())
            #menu.popup(menu_helper, event)
            menu.caller=menu_helper
            menu.menu.popup(None, None, win.position_menu, event.button,
                            event.time)
        else:
            menu.popup(menu_helper, event)
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

pfd=pango.FontDescription('Sans bold %d' % (wsize-16))
layout=can.create_pango_layout('?')
layout.set_font_description(pfd)

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
    style=widget.get_style()
    gc=style.bg_gc[g.STATE_NORMAL]
    #widget.window.draw_rectangle(gc, 1, 0, 0, width, height)
    #print dir(widget.window)
    try:
        area=None
        style.apply_default_background(widget.window, g.TRUE, g.STATE_NORMAL,
                                       area,
                                       0, 0, width, height)
    except:
        #print sys.exc_info()[:2]
        widget.window.draw_rectangle(gc, 1, 0, 0, width, height)

    act=stats.getCurrent(iface)
    #gc=widget.get_style().bg_gc[g.STATE_NORMAL]

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

    else:
        tmp=gc.foreground
        gc.foreground=red
        w, h=layout.get_pixel_size()
        x=(width-w)/2
        y=(height-h)/2
        widget.window.draw_layout(gc, x, y, layout)
        gc.foreground=tmp
        
    
can.connect('expose_event', expose)

def update():
    stats.update()
    can.queue_draw()
    return 1

tag=g.timeout_add(1000, update)

win.show_all()
rox.mainloop()

for pid in pids:
    os.waitpid(pid, 0)
