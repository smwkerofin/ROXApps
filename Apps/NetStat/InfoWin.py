import os

import findrox
from rox import g
#from gtk import *
#from GDK import *
import webbrowser

class InfoWin(g.Dialog):
    def __init__(self, program, purpose, version, author, website):
        g.Dialog.__init__(self)
        self.website=website

        def close(iw, event=None, data=None):
            iw.hide()

        self.connect("delete_event", close)

        table=g.Table(5, 2)
        self.vbox.pack_start(table)

        label=g.Label("Program")
        table.attach(label, 0, 1, 0, 1)

        frame=g.Frame()
        frame.set_shadow_type(g.SHADOW_IN)
        table.attach(frame, 1, 2, 0, 1)

        label=g.Label(program)
        frame.add(label)

        label=g.Label("Purpose")
        table.attach(label, 0, 1, 1, 2)

        frame=g.Frame()
        frame.set_shadow_type(g.SHADOW_IN)
        table.attach(frame, 1, 2, 1, 2)

        label=g.Label(purpose)
        frame.add(label)

        label=g.Label("Version")
        table.attach(label, 0, 1, 2, 3)

        frame=g.Frame()
        frame.set_shadow_type(g.SHADOW_IN)
        table.attach(frame, 1, 2, 2, 3)

        label=g.Label(version)
        frame.add(label)

        label=g.Label("Author")
        table.attach(label, 0, 1, 3, 4)

        frame=g.Frame()
        frame.set_shadow_type(g.SHADOW_IN)
        table.attach(frame, 1, 2, 3, 4)

        label=g.Label(author)
        frame.add(label)

        label=g.Label("Web site")
        table.attach(label, 0, 1, 5, 6)

        button=g.Button(website)
        table.attach(button, 1, 2, 5, 6)

        def goto_website(widget, iw):
            webbrowser.open(iw.website)

        button.connect("clicked", goto_website, self)

        hbox=self.action_area

        button=g.Button("Dismiss")
        hbox.pack_start(button)

        def dismiss(widget, iw):
            iw.hide()

        button.connect("clicked", dismiss, self)
        button.show()

        self.vbox.show_all()


