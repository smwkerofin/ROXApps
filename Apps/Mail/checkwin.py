#import rox.loading
from rox import g
from Checker import Checker
from my_support import *
import rox

class CheckWin(g.Dialog):
    def __init__(self, checks):
        # rox.loading.XDSLoader.__init__(self, None)
        self.current=None
        self.all=checks
        
        g.Dialog.__init__(self)
        self.set_title('Setup checking programs')

        self.vbox.set_spacing(4)
        hbox=g.HBox(spacing=4)
        self.vbox.pack_start(hbox)

        scrw=g.ScrolledWindow()
        scrw.set_size_request(-1, 100)
        hbox.pack_start(scrw)

        titles=['Program', 'Interval']
        self.progs=g.CList(2, titles)
        self.progs.set_column_auto_resize(0, g.TRUE)
        self.progs.set_column_auto_resize(1, g.TRUE)
        if checks!=None:
            for p in checks:
                c=p.getCommand()
                s='%.2f' % p.getInterval()
                v=[c, s]
                # print v, c, s
                row=self.progs.append(v)
                self.progs.set_row_data(row, p)
            
        def sel_row(list, row, column, event, data):
            checkwin=data
            checkwin.select_row(list, row, column)
        def unsel_row(list, row, column, event, data):
            checkwin=data
            checkwin.unselect_row(list, row, column)
        self.progs.connect('select-row', sel_row, self)
        self.progs.connect('unselect-row', unsel_row, self)
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

        def cancel(button, self):
            self.destroy()

        button=g.Button("Cancel")
        button.connect('clicked', cancel, self)
        hbox.pack_end(button, expand=g.FALSE)
        
        def add(button, self):
            self.add()

        button=g.Button("Add")
        button.connect('clicked', add, self)
        hbox.pack_end(button, expand=g.FALSE)
        
        def remove(button, self):
            self.remove()

        button=g.Button("Remove")
        button.connect('clicked', remove, self)
        hbox.pack_end(button, expand=g.FALSE)
        self.rembut=button
        self.rembut.set_sensitive(g.FALSE)
        
        def update(button, self):
            self.update()

        button=g.Button("Update")
        button.connect('clicked', update, self)
        hbox.pack_end(button, expand=g.FALSE)
        
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

    def add(self):
        checker=Checker('true', 300.)
        self.all.append(checker)
        row=self.progs.append([checker.getCommand(), "%f" % checker.getInterval()])
        self.progs.set_row_data(row, checker)

    def remove(self):
        pass

    def update(self):
        print self, self.current
        if self.current!=None:
            self.current.setCommand(self.cmd.get_text())
            self.current.setInterval(float(self.int.get_text()))
            row=self.progs.find_row_from_data(self.current)
            self.progs.set_text(row, 0, self.current.getCommand())
            self.progs.set_text(row, 1, "%f" % checker.getInterval())
            self.current.schedule()
            fname=rox.choices.save('Mail', 'check.xml')
            writeCheckers(self.all, fname)

