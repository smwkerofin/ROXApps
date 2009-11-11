import os
import sys
import subprocess

import gobject

import rox
import rox.choices
import rox.tasks
import rox.OptionsBox

check_hosts=None
check_dns=None
check_update=None

window=None

def build_extlist(box, node, label, option):
    """<extlist name='...' label='...' edit='yes|no' selection='single|none|multiple'>Tooltip</extlist>"""
    
    edit=rox.OptionsBox.bool_attr(node, 'edit')
    reorder=rox.OptionsBox.bool_attr(node, 'reorder')
    extend=True
    select=rox.OptionsBox.str_attr(node, 'selection', 'single')
		
    cont=rox.g.VBox(False, 4)
    cont._rox_lib_expand=True
    
    if label:
        label_wid = rox.g.Label(label)
        cont.pack_start(label_wid, False, True, 0)
        label_wid.show()
                        
    swin = rox.g.ScrolledWindow()
    swin.set_border_width(4)
    swin.set_policy(rox.g.POLICY_NEVER, rox.g.POLICY_ALWAYS)
    swin.set_shadow_type(rox.g.SHADOW_IN)
    cont.pack_start(swin, True, True, 0)
    
    model = rox.g.ListStore(str)
    view = rox.g.TreeView(model)
    swin.add(view)
    
    selection=view.get_selection()
    if select=='none':
        selection.set_mode(rox.g.SELECTION_NONE)
    elif select=='multiple':
        selection.set_mode(rox.g.SELECTION_MULTIPLE)
    else:
        selection.set_mode(rox.g.SELECTION_SINGLE)
        select='single'
        
    if reorder:
        view.set_reorderable(True)
        
    def cell_edited(cell, path, new_text, col):
        #print 'in cel_edited', cell, path, new_text, col
        iter=model.get_iter_from_string(path)
        model.set(iter, col, new_text)
        box.check_widget(option)
			
    cell = rox.g.CellRendererText()
    #print 'edit', edit
    if edit:
        #print 'make', cell, 'editable'
        cell.set_property('editable', True)
        cell.connect('edited', cell_edited, 0)
    column = rox.g.TreeViewColumn('Host', cell, text = 0)
    view.append_column(column)

    def add(widget, box):
        iter=model.append()
        model.set(iter, 0, '127.0.0.1')
        if select=='single':
            view.get_selection().select_iter(iter)
        box.check_widget(option)

    hbox=rox.g.HBox(False, 2)
    cont.pack_start(hbox, False)
        
    but=rox.g.Button(stock=rox.g.STOCK_ADD)
    but.connect('clicked', add, box)
    hbox.pack_start(but, False)            

    box.may_add_tip(swin, node)

    def get():
        v=[]
        iter=model.get_iter_first()
        while iter:
            var=model.get_value(iter, 0)
            v.append(var)
            
            iter=model.iter_next(iter)
        return v

    def set():
        model.clear()
        for v in option.list_value:
            iter=model.append()
            model.set(iter, 0, v)

    box.handlers[option]=(get, set)

    return [cont]


def init_options():
    global check_hosts, check_dns, check_update

    rox.OptionsBox.widget_registry['extlist']=build_extlist

    check_hosts=rox.options.ListOption('check_hosts', ('www.google.com',))
    check_dns=rox.options.Option('check_dns', True)
    check_update=rox.options.Option('check_update', 60)

    rox.app_options.add_notify(options_changed)

def options_changed():
    global window
    
    if check_hosts.has_changed:
        if window:
            window.load_hosts()

    if check_dns.has_changed:
        if window:
            window.load_hosts()

    if check_update.has_changed:
        if window:
            window.set_update()

def open():
    global window
    
    if not window:
        window=Window()
        window.show()
    else:
        window.present()

def dns_hosts():
    try:
        for line in file('/etc/resolv.conf', 'r').readlines():
            l=line.strip()
            if l.startswith('nameserver '):
                ns, addr=l.split(' ', 1)
                yield addr
    except Exception, ex:
        print ex

def hosts():
    return iter(check_hosts.list_value)

class Window(rox.Window):
    titles=('Host', 'ms')
    NORMAL='black'
    SENDING='blue'
    NO_REPLY='red'
    SLOW='orange'
    
    def __init__(self):
        rox.Window.__init__(self)

        self.set_title('Connectivity')

        swin=rox.g.ScrolledWindow()
        swin.set_size_request(160, 100)
        swin.set_policy(rox.g.POLICY_NEVER, rox.g.POLICY_AUTOMATIC)
        self.add(swin)

        self.store=rox.g.ListStore(str, str, str)
        self.view=rox.g.TreeView(self.store)

        def str_col(view, icol, title):
            cell=rox.g.CellRendererText()
            col=rox.g.TreeViewColumn(title, cell, text=icol, foreground=2)
            col.set_sort_column_id(icol)
            view.append_column(col)


        for i in range(len(self.titles)):
            str_col(self.view, i, self.titles[i])

        sel=self.view.get_selection()
        sel.set_mode(rox.g.SELECTION_NONE)

        swin.add(self.view)

        self.load_hosts()

        swin.show_all()

        self.update_tag=None
        self.rate=None

        self.connect('show', self.on_show)
        self.connect('destroy', self.on_close)
        
    def load_hosts(self):
        self.store.clear()
        
        if check_dns.int_value:
            for host in dns_hosts():
                self.add_host(host)

        for host in hosts():
            self.add_host(host)

    def add_host(self, host):
        it=self.store.append()
        self.store[it][0]=host
        self.store[it][1]='sending'
        self.store[it][2]=self.SENDING

    def set_update(self, sec=None):
        if not sec:
            sec=check_update.int_value

        if self.update_tag:
            gobject.source_remove(self.update_tag)

        self.update_tag=gobject.timeout_add(sec*1000, self.start_update)
        self.rate=sec

    def start_update(self):
        #print 'start_update'
        for it in self.store:
            self.start_ping(it[0])
            it[2]=self.SENDING
        return True

    def start_ping(self, host):
        #print 'ping %s' % host

        pinger=Ping.get(host)
        rox.tasks.Task(pinger.task(self))

    def ping_done(self, host, avg_time):
        for it in self.store:
            if it[0]==host:
                if avg_time is None:
                    it[1]='no reply'
                    it[2]=self.NO_REPLY
                else:
                    it[1]='%.1f' % avg_time
                    it[2]=self.NORMAL if avg_time<1000 else self.SLOW

    def on_close(self, ignored):
        global window
        
        if self.update_tag:
            gobject.source_remove(self.update_tag)

        if window==self:
            window=None

    def on_show(self, ignored):
        self.start_update()
        self.set_update()

class Ping(object):
    def __init__(self, host, interval):
        self.host=host
        if not interval:
            interval=check_update.int_value

        tlimit=interval/2
        if tlimit<5:
            tlimit=5

        cmd=self.get_cmd(host, tlimit)

        self.proc=subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,
                                   bufsize=1)

    def get_cmd(self, host, tlimit):
        pass

    def parse_output(self, output):
        pass

    def task(self, control):
        self.output=''
        while True:
            yield rox.tasks.InputBlocker(self.proc.stdout)
            line=self.proc.stdout.readline()
            if not line:
                break
            self.output+=line

        while self.proc.poll():
            yield rox.tasks.TimeoutBlocker(5)
            
        self.proc=None
        control.ping_done(self.host, self.parse_output(self.output))

    @classmethod
    def get(klass, host, interval=None):
        if sys.platform=='linux2':
            return LinuxPing(host, interval)

        raise Exception, 'no implementation for %s' % sys.platform

class LinuxPing(Ping):
    def __init__(self, host, interval):
        Ping.__init__(self, host, interval)
        
    def get_cmd(self, host, tlimit):
        return 'ping -c 2 -w %d %s' % (tlimit, host)

    def parse_output(self, output):
        ttime=0
        nrep=0
        for line in output.split('\n'):
            l=line.strip()
            if 'time=' in l:
                words=l.split()
                if words[-2].startswith('time='):
                    s, stime=words[-2].split('=', 1)
                    ms=float(stime)
                    ttime+=ms
                    nrep+=1
        if nrep>0:
            return ttime/nrep

