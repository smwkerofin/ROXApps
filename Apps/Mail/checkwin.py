from gtk import *
from Checker import Checker
from my_support import *
import rox

TARGET_URI_LIST=0
targets=[('text/uri-list', 0, TARGET_URI_LIST)]
text_uri_list=atom_intern("text/uri-list")

class CheckWin(GtkDialog):
    def __init__(self, checks):
        self.current=None
        self.all=checks
        
        GtkDialog.__init__(self)
        self.set_title('Setup checking programs')

        self.vbox.set_spacing(4)
        hbox=GtkHBox(spacing=4)
        self.vbox.pack_start(hbox)

        scrw=GtkScrolledWindow()
        hbox.pack_start(scrw)

        titles=['Program', 'Interval']
        self.progs=GtkCList(2, titles)
        self.progs.set_column_auto_resize(0, TRUE)
        self.progs.set_column_auto_resize(1, TRUE)
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

        hbox=GtkHBox(spacing=4)
        self.vbox.pack_start(hbox)

        label=GtkLabel("Command")
        hbox.pack_start(label)

        self.cmd=GtkEntry()
        hbox.pack_start(self.cmd)
        
        hbox=GtkHBox(spacing=4)
        self.vbox.pack_start(hbox)

        label=GtkLabel("Interval")
        hbox.pack_start(label)

        self.int=GtkEntry()
        hbox.pack_start(self.int)
        
        hbox=self.action_area

        def add(button, self):
            self.add()

        button=GtkButton("Add")
        button.connect('clicked', add, self)
        hbox.pack_end(button, expand=FALSE)
        
        def remove(button, self):
            self.remove()

        button=GtkButton("Remove")
        button.connect('clicked', remove, self)
        hbox.pack_end(button, expand=FALSE)
        self.rembut=button
        self.rembut.set_sensitive(FALSE)
        
        def update(button, self):
            self.update()

        button=GtkButton("Update")
        button.connect('clicked', update, self)
        hbox.pack_end(button, expand=FALSE)
        
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

