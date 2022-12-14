# $Id: gui.py,v 1.22 2008/07/14 16:41:27 stephen Exp $

import os
import sys
import string
import math

import findrox; findrox.version(2, 0, 2)

import rox
import rox.choices

from rox import g
import pango, gobject

import netstat
import rox.Menu
import rox.applet
import rox.options
import rox.InfoWin

from sockwin import SocketsWindow
import usage
import check

#import gc; gc.set_debug(gc.DEBUG_LEAK)

#print >>sys.stderr, sys.argv
if len(sys.argv)>1:
    xid=long(sys.argv[1])
else:
    xid=None

app_dir=os.path.dirname(sys.argv[0])

stats=netstat.NetStat()
#print stats

# Defaults
iface='ppp0'
select_cmd=''
adjust_cmd=''
levels=[500, 50, 1, 0]
levels2=[4096, 256, 1, 0]
wsize=48

DISPLAY_ARROWS='arrows'
DISPLAY_CHART='chart'

pids=[]

# Load config
rox.setup_app_options('NetStat', site='kerofin.demon.co.uk')

interface=rox.options.Option('interface', iface)
connect=rox.options.Option('connect', select_cmd)
disconnect=rox.options.Option('disconnect', adjust_cmd)

medium_level=rox.options.Option('medium', levels[1])
high_level=rox.options.Option('high', levels[0])
medium_K=rox.options.Option('medium_K', levels2[1])
high_K=rox.options.Option('high_K', levels2[0])

win_size=rox.options.Option('wsize', wsize)
display=rox.options.Option('display', DISPLAY_ARROWS)

period=rox.options.Option('period', 30)
rx_limit=rox.options.Option('rx_limit', 30)
tx_limit=rox.options.Option('tx_limit', 30)
total_limit=rox.options.Option('total_limit', 30)


ifdisp=None
win=None
uwin=None

def options_changed():
    global iface, select_cmd, uwin
    
    #print 'in options_changed()'
    if interface.has_changed:
        #print 'iface=%s' % interface.value
        iface=interface.value
        if ifdisp:
            ifdisp.set_text(iface)
        if win:
            win.set_title(iface)
        if uwin:
            uwin.set_interface(iface)
            
    if connect.has_changed:
        select_cmd=connect.value
    if disconnect.has_changed:
        adjust_cmd=disconnect.value
    if medium_level.has_changed:
        levels[1]=medium_level.int_value
    if high_level.has_changed:
        levels[0]=high_level.int_value
    if medium_K.has_changed:
        levels2[1]=medium_K.int_value
    if high_K.has_changed:
        levels2[0]=high_K.int_value
    if win_size.has_changed and win:
        win.set_size_request(win_size.int_value, win_size.int_value)

    if period.has_changed and uwin:
        uwin.set_period(period.int_value)

    if (rx_limit.has_changed or tx_limit.has_changed or
        total_limit.has_changed) and uwin:
        uwin.set_limits(rx_limit.int_value, tx_limit.int_value,
                        total_limit.int_value)

rox.app_options.add_notify(options_changed)
check.init_options()

rox.app_options.notify()

if xid is None:
    win=rox.Window()
    win.set_title(iface)
else:
    win=rox.applet.Applet(xid)

#win.connect('destroy', g.mainquit)

# Choose a nice small size for our applet...
win.set_size_request(wsize, wsize)
win.set_border_width(2)

style=win.get_style()

vbox=g.VBox()
win.add(vbox)

# Our drawing canvas
can=g.DrawingArea()
vbox.pack_start(can)

ifdisp=g.Label(iface)
vbox.pack_start(ifdisp, False, False)

# Menu
rox.Menu.set_save_name('NetStat')
menu=rox.Menu.Menu('main', [
    rox.Menu.Action(_('Info'), 'show_info', stock=g.STOCK_DIALOG_INFO),
    rox.Menu.Separator(),
    rox.Menu.Action(_('Usage...'), 'show_usage'),
    rox.Menu.SubMenu(_('Sockets'), [
      rox.Menu.Action(_('Show active...'), 'show_active'),
      rox.Menu.Action(_('Show all..'), 'show_all')
    ]),
    rox.Menu.Action(_('Connectivity...'), 'check_conn'),
    rox.Menu.Action(_('Options...'), 'edit_options',
                    stock=g.STOCK_PREFERENCES),
    rox.Menu.Separator(),
    rox.Menu.Action(_('Quit'), 'do_quit', stock=g.STOCK_QUIT)
    ])

class MenuHelper:
    def show_info(unused):
        import rox.InfoWin
        rox.InfoWin.infowin('NetStat')
    def do_quit(unused):
        if uwin:
            uwin.hide()
            #rox.toplevel_unref()
        rox.toplevel_unref()
    def edit_options(unused):
        rox.edit_options()
    def show_active(unused):
        w=SocketsWindow(stats)
        w.show()
    def show_all(unused):
        w=SocketsWindow(stats, 1)
        w.show()

    def show_usage(unused):
        uwin.show()

    def check_conn(self):
        check.open()

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
black=cmap.alloc_color('#000000')
blue=cmap.alloc_color('#0000ff')
green=cmap.alloc_color('#00bb00')
red=cmap.alloc_color('#ff0000')
high=cmap.alloc_color('#00ff00')
medium=green
low=cmap.alloc_color('#007700')
off=black

colours=(high, medium, low, off)

pfd=pango.FontDescription('Sans bold %d' % (wsize-16))
layout=can.create_pango_layout('?')
layout.set_font_description(pfd)
pfd=pango.FontDescription('Sans bold 6')
slayout=can.create_pango_layout('?')
slayout.set_font_description(pfd)

def draw_arrow(drawable, gc, pts, act, mid, top, bot):
    tmp=gc.foreground
    col=red
    for i in range(len(levels)):
        if act>=levels[i]:
            col=colours[i]
            break
    gc.foreground=col
    drawable.draw_polygon(gc, 1, pts)
    gc.foreground=black
    s=str(act)
    try:
        slayout.set_text(s, len(s))
    except:
        slayout.set_text(s)
    w, h=slayout.get_pixel_size()
    x=mid-w/2
    y=top+((top+bot)/2-h)/2
    #print mid, w, x
    drawable.draw_layout(gc, x, y, slayout)
    gc.foreground=tmp

def hsize(val):
    if val>2*1024*1024:
        return '%dM' % (val/1024/1024)
    if val>2*1024:
        return '%dK' % (val/1024)
    return '%d' % val

def draw_arrow2(drawable, gc, pts, act, mid, top, bot):
    tmp=gc.foreground
    col=red
    for i in range(len(levels2)):
        if act>=levels2[i]:
            col=colours[i]
            break
    gc.foreground=col
    drawable.draw_polygon(gc, 1, pts)
    gc.foreground=black
    s=hsize(act)
    try:
        slayout.set_text(s, len(s))
    except:
        slayout.set_text(s)
    w, h=slayout.get_pixel_size()
    x=mid-w/2
    y=top #+((top+bot)/2-h)/2
    #print mid, w, x
    #print top, bot, h, y
    drawable.draw_layout(gc, x, y, slayout)
    gc.foreground=tmp

def display_arrows(widget, act):
    (x, y, width, height)=widget.get_allocation()
    style=widget.get_style()
    gc=style.bg_gc[g.STATE_NORMAL]

    # Receive
    top=2
    bot=height*5/6-2
    mid=height*5/6/2
    cx=width/4
    left=2
    right=width/2-2
    pts=((cx-2,top), (cx-2,mid), (left,mid), (cx,bot), (right,mid),
         (cx+2,mid), (cx+2,top))
    if act[2]<0:
        draw_arrow(widget.window, gc, pts, act[0], cx, mid, bot)
    else:
        draw_arrow2(widget.window, gc, pts, act[2], cx, bot, height)
        
    # Transmit
    top=2+height/6
    bot=height-2
    mid=top+height*5/6/2
    cx=3*width/4
    left=width/2+2
    right=width-2
    pts=((cx-2,bot), (cx-2,mid), (left,mid), (cx,top), (right,mid),
         (cx+2,mid), (cx+2,bot))
    if act[3]<0:
        draw_arrow(widget.window, gc, pts, act[1], cx, top, mid)
    else:
        draw_arrow2(widget.window, gc, pts, act[3], cx, 2, top)

def draw_chart(drawable, gc, box, history, ind, levels, max):
    #print box, history, ind, levels, max
    top, bot, left, right=box

    for i in range(len(history)):
        val=history[-i-1][ind]
        x=right-i
        y0=bot
        y1=bot-int(val/float(max)*(bot-top))
        #print bot-top, max, val/float(max), int(val/float(max)*(bot-top))
        #print i, val, x, y0, y1
        gc.foreground=low
        drawable.draw_line(gc, x, y0, x, y1)

        if val>levels[1]:
            gc.foreground=medium
            y0=bot-int(levels[1]/float(max)*(bot-top))
            drawable.draw_line(gc, x, y0, x, y1)
            if val>levels[0]:
                gc.foreground=high
                y0=bot-int(levels[0]/float(max)*(bot-top))
                drawable.draw_line(gc, x, y0, x, y1)

    gc.foreground=black
    drawable.draw_line(gc, left, bot, right, bot)

def pretty_max(mval, bin=False):
    if mval<=0:
        return 0

    if bin:
        e=int(math.log(mval)/math.log(2))
        b=2**e
        if mval<=b:
            return b
        return 2*b

    else:
        e=int(math.log10(mval))
        b=10**e
        if mval<=b:
            return b
        if mval<=b*2:
            return b*2
        if mval<=b*5:
            return b*5
        return b*10

    return mval

history=[]
hmax=0
def display_chart(widget, act):
    global hmax, history
    
    (x, y, width, height)=widget.get_allocation()
    style=widget.get_style()
    gc=style.bg_gc[g.STATE_NORMAL]
    tmpfg=gc.foreground

    if hmax<1:
        if act[2]<0 or act[3]<0:
            hmax=levels[2]
        else:
            hmax=levels2[2]

    if act[2]<0 or act[3]<0:
        vals=(act[0], act[1])
        vind=0
        l=levels
    else:
        vals=(act[2], act[3])
        vind=2
        l=levels2

    if act[vind]>hmax: hmax=act[vind]
    if act[vind+1]>hmax: hmax=act[vind+1]

    if len(history)>0:
        history.append(act)
    else:
        history.append((0, 0, 0, 0))
    if len(history)>width:
        p=len(history)-width
        history=history[p:]

        max=history[0][vind]
        for v in history:
            if v[vind]>max: max=v[vind]
            if v[vind+1]>max: max=v[vind+1]

        if max<hmax/2:
            hmax=max

    pmax=pretty_max(hmax, vind>1)

    # Transmit
    top=2
    bot=height/2-2
    left=0
    right=width
    if pmax>0:
        try:
            draw_chart(widget.window, gc, (top, bot, left, right),
                       history, vind+1, l, pmax)
        except:
            pass
    slayout.set_text('Tx')
    gc.foreground=black
    widget.window.draw_layout(gc, left, top, slayout)

    # Receive
    top=height/2+2
    bot=height-2
    left=0
    right=width
    if pmax>0:
        try:
            draw_chart(widget.window, gc, (top, bot, left, right),
                       history, vind+0, l, pmax)
        except:
            pass
    slayout.set_text('Rx')
    gc.foreground=black
    widget.window.draw_layout(gc, left, top, slayout)
    
    if vind>1:
        s=hsize(pmax)
    else:
        s='%dp' % pmax
    #print hmax, s, history[-1]
    try:
        slayout.set_text(s, len(s))
    except:
        slayout.set_text(s)
    w, h=slayout.get_pixel_size()
    x=width-w
    y=0
    gc.foreground=black
    widget.window.draw_layout(gc, x, y, slayout)

    gc.foreground=tmpfg

display_modes={DISPLAY_ARROWS: display_arrows, DISPLAY_CHART: display_chart}
    
def expose(widget, event):
    (x, y, width, height)=widget.get_allocation()
    style=widget.get_style()
    gc=style.bg_gc[g.STATE_NORMAL]

    try:
        area=None
        style.apply_default_background(widget.window, True, g.STATE_NORMAL,
                                       area,
                                       0, 0, width, height)
    except:
        widget.window.draw_rectangle(gc, 1, 0, 0, width, height)

    act=stats.getCurrent(iface)

    if act and len(act)>1:
        #print display.value, display_modes
        try:
            #print display_modes[display.value]
            dfunc=display_modes[display.value]
        except:
            dfunc=display_arrows
        #print dfunc
        dfunc(widget, act)

    else:
        tmp=gc.foreground
        gc.foreground=red
        w, h=layout.get_pixel_size()
        x=(width-w)/2
        y=(height-h)/2
        widget.window.draw_layout(gc, x, y, layout)
        gc.foreground=tmp
        
def resize(widget, event, udata=None):
    global slayout
    
    (x, y, width, height)=widget.get_allocation()
    size=(width+height)/2/8-2
    if size<6:
        size=6
    elif size>15:
        size=15
    #print width, height, size
    pfd=pango.FontDescription('Sans %d' % size)
    slayout.set_font_description(pfd)
    
can.connect('expose_event', expose)
can.connect('configure_event', resize)

def update():
    global stats, can
    #print stats
    if stats:
        stats.update()
    if can:
        can.queue_draw()
    return True

#print rox._toplevel_windows
uwin=usage.get_window(interface.value)
uwin.set_period(period.int_value)
uwin.set_limits(rx_limit.int_value, tx_limit.int_value, total_limit.int_value)
uwin.do_update()
#print rox._toplevel_windows
if xid is None:
    rox.toplevel_unref()

tag=gobject.timeout_add(1000, update)
gobject.timeout_add(60*1000, uwin.do_update)

win.show_all()
try:
    rox.mainloop()
finally:
    print 'saving on exit'
    uwin.on_exit()

for pid in pids:
    os.waitpid(pid, 0)
