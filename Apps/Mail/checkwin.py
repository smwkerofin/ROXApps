#import rox.loading
from rox import g
import Checker
from my_support import *
import rox

ADD = 1
DELETE = 2
UPDATE = 3

COMMAND = 0
INTERVAL = 1
CHECKER = 2

class CheckWin(g.Dialog):
    def __init__(self, checks):
        # rox.loading.XDSLoader.__init__(self, None)
        self.current=None
        if checks:
            self.all=checks
        else:
            self.all=[]
        
        g.Dialog.__init__(self)
        self.set_title('Setup checking programs')

        self.vbox.set_spacing(4)
        hbox=g.HBox(spacing=4)
        self.vbox.pack_start(hbox)

        scrw=g.ScrolledWindow()
        scrw.set_size_request(-1, 200)
        scrw.set_border_width(4)
        scrw.set_policy(g.POLICY_NEVER, g.POLICY_ALWAYS)
        scrw.set_shadow_type(g.SHADOW_IN)
        hbox.pack_start(scrw)

        self.model=g.ListStore(str, str, object)
        view=g.TreeView(self.model)
        self.progs=view

        cell=g.CellRendererText()
        column = g.TreeViewColumn('Program', cell, text = COMMAND)
        view.append_column(column)
        
        cell=g.CellRendererText()
        column = g.TreeViewColumn('Interval', cell, text = INTERVAL)
        view.append_column(column)
        
        for p in self.all:
                c=p.getCommand()
                s='%.2f' % p.getInterval()
                iter=self.model.append()
                self.model.set(iter, COMMAND, c, INTERVAL, s, CHECKER, p)
            
        self.progs.connect('row-activated', self.active_row)
        scrw.add(self.progs)

        hbox=g.HBox(spacing=4)
        self.vbox.pack_start(hbox)

        label=g.Label("Command")
        hbox.pack_start(label)

        self.cmd=g.Entry()
        hbox.pack_start(self.cmd)
        
        hbox=g.HBox(spacing=4)
        self.vbox.pack_start(hbox)

        label=g.Label("Interval")
        hbox.pack_start(label)

        self.int=g.Entry()
        hbox.pack_start(self.int)
        
        hbox=self.action_area

        self.add_button(g.STOCK_ADD, ADD)
        self.add_button(g.STOCK_DELETE, DELETE)
        self.add_button(g.STOCK_SAVE, UPDATE)
        self.add_button(g.STOCK_CLOSE, g.RESPONSE_CANCEL)

        self.set_default_response(g.RESPONSE_CANCEL)

        def response(self, resp):
            if resp == UPDATE:
                self.update()
            elif resp == DELETE:
                self.remove()
            elif resp == ADD:
                self.add()
            else:
                self.destroy()
        self.connect('response', response)

        self.vbox.show_all()
        self.action_area.show_all()

    def select_row(self, list, row, col):
        checker=list.get_row_data(row)
        self.current=checker
        self.cmd.set_text(checker.getCommand())
        self.int.set_text("%.0f" % checker.getInterval())

    def unselect_row(self, list, row, col):
        self.current=None
        self.cmd.set_text('')
        self.int.set_text('')

    def active_row(self, view, path, col, udata=None):
        iter=self.model.get_iter(path)
        c=self.model.get_value(iter, CHECKER)
        self.current=c
        self.cmd.set_text(c.getCommand())
        self.int.set_text("%.0f" % c.getInterval())

    def add(self):
        cmd=self.cmd.get_text()
        try:
            intv=float(self.int.get_text())
        except:
            intv=300.
        checker=Checker.Checker(cmd, intv)
        self.all.append(checker)
        iter=self.model.append()
        self.model.set(iter, COMMAND,checker.getCommand() ,
                       INTERVAL, "%f" % checker.getInterval(),
                       CHECKER, checker)
        checker.schedule()

    def remove(self):
        sel=self.progs.get_selection()
        model, iter=sel.get_selected()
        if iter:
            c=model.get_value(iter, CHECKER)
            i=self.all.index(c)
            del self.all[i]
            model.remove(iter)

    def update(self):
        #print self, self.current
        if self.current!=None:
            self.current.setCommand(self.cmd.get_text())
            self.current.setInterval(float(self.int.get_text()))

            iter=self.model.get_iter_first()
            while iter:
                c=self.model.get_value(iter, CHECKER)
                if c==self.current:
                    break
            if iter:
                self.model.set(iter, COMMAND, self.current.getCommand(),
                               INTERVAL, self.current.getInterval())
                self.current.schedule()
            
        fname=rox.choices.save('Mail', 'check.xml')
        Checker.writeCheckers(self.all, fname)

