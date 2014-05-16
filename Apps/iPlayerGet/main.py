import os
import sys
import time
import StringIO

import findrox
findrox.version(2, 0, 5)
import rox
import rox.processes
#import rox.filer
import rox.tasks
import rox.choices

outdir=os.path.join(os.path.expanduser('~'), 'mp3', 'Radio', 'iPlayer')
print outdir
if not os.path.isdir(outdir):
    os.makedirs(outdir)

def_programs=("Just a Minute", "I'm Sorry I Haven't A Clue", 
          "Cabin Pressure", "Giles Wemmbley Hogg", 
          "Milton Jones", "Old Harry", "Goon Show", "Now Show",
          "Bleak Expectations", 'Terry Pratchett',
          'Doctor Who', 'Neverwhere')
#programs=list(def_programs)
#print programs

class Lookup(rox.processes.PipeThroughCommand):
    def __init__(self, manager, programme, ostream):
        cmd='get_iplayer --type radio "%s"' % programme
        print cmd
        rox.processes.PipeThroughCommand.__init__(self, cmd, None, ostream)
        self.manager=manager

    def child_died(self, status):
        rox.processes.PipeThroughCommand.child_died(self, status)
        self.manager.lookup_finished(status)

class Manager(object):
    def __init__(self, window, programs):
        self.window=window
        self.programs=programs
        self.answers=[]
        self.prog_iter=iter(self.programs)

        self.next_prog()

    def next_prog(self):
        try:
            prog=self.prog_iter.next()
            print 'lookup', prog
            self.window.status('Lookup "%s"' % prog)
            self.ostream=StringIO.StringIO()
            l=Lookup(self, prog, self.ostream)
            l.start()
            #l.wait()
        except StopIteration:
            self.window.status('Lookup finished.')
            self.send_answer()

    def send_answer(self):
        self.window.lookup_finished(self.answers)

    def lookup_finished(self, status):
        #print status
        #print self.ostream.getvalue()
        for line in self.ostream.getvalue().split('\n'):
            if ':' in line:
                #print line.strip()
                front, tail=line.strip().split(':', 1)
                try:
                    idno=int(front)
                except ValueError:
                    continue
                self.answers.append((idno, tail.strip()))
                #print idno, tail

        self.next_prog()
    
class Fetch(rox.processes.Process):
    def __init__(self, window, progid):
        rox.processes.Process.__init__(self)
        self.window=window
        self.progid=progid

    def child_died(self, status):
        self.window.fetch_finished(status)

    def child_run(self):
        global outdir
        os.execvp(os.path.join(rox.app_dir, 'helper.py'),
                  ('helper.py', outdir, str(self.progid)))
        os._exit(1)

class Window(rox.Window):
    def __init__(self):
        global programs
        
        rox.Window.__init__(self)
        self.set_title('iPlayer get')

        self.make_gui()

        self.programs=tuple(load_filters())
        #self.results={}

    def make_gui(self):
        vbox=rox.g.VBox()
        self.add(vbox)
        
        swin=rox.g.ScrolledWindow()
        vbox.pack_start(swin, True, True, 2)

        self.model=rox.g.ListStore(str, str)
        view=rox.g.TreeView(self.model)
        view.set_size_request(-1, 240)
        swin.add(view)
        self.view=view

        cell=rox.g.CellRendererText()
        column=rox.g.TreeViewColumn('Id', cell, text=0)
        view.append_column(column)

        cell=rox.g.CellRendererText()
        column=rox.g.TreeViewColumn('Title', cell, text=1)
        view.append_column(column)

        lbl=rox.g.Label()
        vbox.pack_start(lbl, True, True, 2)
        self.update_line=lbl
        
        lbl=rox.g.Label()
        vbox.pack_start(lbl, True, True, 2)
        self.msg_line=lbl
        
        bbar=rox.g.HButtonBox()
        vbox.pack_end(bbar, False, False, 2)
        bbar.set_layout(rox.g.BUTTONBOX_END)
        but=rox.g.Button('Filters')
        but.connect('clicked', self.do_filters)
        bbar.pack_start(but, False, False, 2)
        but=rox.g.Button('_Fetch')
        but.connect('clicked', self.do_fetch)
        bbar.pack_start(but, False, False, 2)
        but=rox.g.Button(stock=rox.g.STOCK_REFRESH)
        but.connect('clicked', self.do_refresh)
        bbar.pack_start(but, False, False, 2)

        vbox.show_all()

    def status(self, text, update=False):
        self.msg_line.set_text(text)
        if update:
            self.update_line.set_text(text)
        while rox.g.events_pending():
            rox.g.main_iteration_do(False)

    def fetch_finished(self, status):
        global outdir
        self.status('Fetch finished')
        #rox.filer.open_dir(outdir)

    def do_fetch(self, button):
        #print 'fetch'
        model, sel=self.view.get_selection().get_selected()
        #print sel
        #print dir(sel)
        if sel:
            #print sel
            idno=model.get(sel, 0)[0]
            fetcher=Fetch(self, idno)
            self.status('Fetch %s' % idno)
            fetcher.start()

    def do_refresh(self, button):
        self.status('Refresh')
        Manager(self, self.programs)

    def do_filters(self, button):
        win=FilterWindow(self, self.programs)
        win.show()

    def lookup_finished(self, answers):
        self.model.clear()
        for idno, title in answers:
            it=self.model.append()
            self.model.set(it, 0, str(idno), 1, title)
        self.status('Last lookup: %s' % time.strftime('%Y-%m-%d %H:%M:%S'),
                    True)

    def filters_done(self, win, filters):
        win.hide()
        win.destroy()

        if filters:
            print filters
            self.programs=filters
            save_filters(self.programs)

def load_filters():
    ipath=rox.choices.load('kerofin.demon.co.uk/iPlayerGet', 'programs')
    print ipath
    if ipath:
        i=open(ipath, 'r')
        filt=[]
        for l in i.readlines():
            filt.append(l.strip())
        return filt
    return list(def_programs)

def save_filters(filters):
    opath=rox.choices.save('kerofin.demon.co.uk/iPlayerGet', 'programs')
    print opath
    if not opath:
        return

    o=open(opath, 'w')
    for filt in filters:
        o.write(filt+'\n')

class FilterWindow(rox.Window):
    def __init__(self, parent, filters):
        rox.Window.__init__(self)
        self.set_title('iPlayer get - filters')

        self.parent_obj=parent

        self.make_gui()
        self.load_list(filters)
        
    def make_gui(self):
        vbox=rox.g.VBox()
        self.add(vbox)
        
        swin=rox.g.ScrolledWindow()
        vbox.pack_start(swin, True, True, 2)

        self.model=rox.g.ListStore(str, str)
        view=rox.g.TreeView(self.model)
        view.set_size_request(-1, 240)
        swin.add(view)
        self.view=view

        cell=rox.g.CellRendererText()
        cell.set_property('editable', True)
        cell.connect('edited', self.text_edited)
        column=rox.g.TreeViewColumn('Id', cell, text=0)
        view.append_column(column)

        bbar=rox.g.HButtonBox()
        vbox.pack_end(bbar, False, False, 2)
        bbar.set_layout(rox.g.BUTTONBOX_END)
        but=rox.g.Button(stock=rox.g.STOCK_ADD)
        but.connect('clicked', self.do_add)
        bbar.pack_start(but, False, False, 2)
        but=rox.g.Button(stock=rox.g.STOCK_REMOVE)
        but.connect('clicked', self.do_remove)
        bbar.pack_start(but, False, False, 2)

        bbar=rox.g.HButtonBox()
        vbox.pack_end(bbar, False, False, 2)
        bbar.set_layout(rox.g.BUTTONBOX_END)
        but=rox.g.Button(stock=rox.g.STOCK_CANCEL)
        but.connect('clicked', self.do_cancel)
        bbar.pack_start(but, False, False, 2)
        but=rox.g.Button(stock=rox.g.STOCK_SAVE)
        but.connect('clicked', self.do_save)
        bbar.pack_start(but, False, False, 2)

        self.show_all()

    def load_list(self, filters):
        self.model.clear()
        for filter in filters:
            it=self.model.append()
            self.model.set(it, 0, filter)

    def do_add(self, button):
        it=self.model.append()
        self.model.set(it, 0, '<NEW>')
        self.view.get_selection().select_iter(it)

    def do_remove(self, button):
        sel=self.view.get_selection()
        model, it=sel.get_selected()
        #print model, it
        if it is not None:
            model.remove(it)

    def do_cancel(self, button):
        self.parent_obj.filters_done(self, None)

    def do_save(self, button):
        filts=[]
        for row in self.model:
            filts.append(row[0])
        self.parent_obj.filters_done(self, filts)

    def text_edited(self, cell, path, new_text):
        it=self.model.get_iter_from_string(path)
        self.model.set(it, 0, new_text)
        

def run():
    win=Window()
    win.show()
    rox.mainloop()



