import os

from gtk import *
from GDK import *

class InfoWin(GtkDialog):
    def __init__(self, program, purpose, version, author, website):
        GtkDialog.__init__(self)
        self.website=website
        self.browser_cmds=["netscape"]

        def close(iw, event=None, data=None):
            iw.hide()

        self.connect("delete_event", close)

        table=GtkTable(5, 2)
        self.vbox.pack_start(table)

        label=GtkLabel("Program")
        table.attach(label, 0, 1, 0, 1)

        frame=GtkFrame()
        frame.set_shadow_type(SHADOW_IN)
        table.attach(frame, 1, 2, 0, 1)

        label=GtkLabel(program)
        frame.add(label)

        label=GtkLabel("Purpose")
        table.attach(label, 0, 1, 1, 2)

        frame=GtkFrame()
        frame.set_shadow_type(SHADOW_IN)
        table.attach(frame, 1, 2, 1, 2)

        label=GtkLabel(purpose)
        frame.add(label)

        label=GtkLabel("Version")
        table.attach(label, 0, 1, 2, 3)

        frame=GtkFrame()
        frame.set_shadow_type(SHADOW_IN)
        table.attach(frame, 1, 2, 2, 3)

        label=GtkLabel(version)
        frame.add(label)

        label=GtkLabel("Author")
        table.attach(label, 0, 1, 3, 4)

        frame=GtkFrame()
        frame.set_shadow_type(SHADOW_IN)
        table.attach(frame, 1, 2, 3, 4)

        label=GtkLabel(author)
        frame.add(label)

        label=GtkLabel("Web site")
        table.attach(label, 0, 1, 5, 6)

        button=GtkButton(website)
        table.attach(button, 1, 2, 5, 6)

        def goto_website(widget, iw):
            child_pid=os.fork()
            if child_pid==-1:
                gdk_beep()
                return
            if child_pid>0:
                return

            for browser in iw.browser_cmds:
                cmd=browser+" "+iw.website
                
                try:
                    error=os.system(cmd)
                except:
                    pass
            os._exit(127)

        button.connect("clicked", goto_website, self)

        hbox=self.action_area

        button=GtkButton("Dismiss")
        hbox.pack_start(button)

        def dismiss(widget, iw):
            iw.hide()

        button.connect("clicked", dismiss, self)
        button.show()

        self.vbox.show_all()

    def add_browser_command(self, cmd):
        self.browser_cmds.insert(0, cmd)
