#!/usr/bin/env python

import findrox
import rox.choices

import os.path
import os
import string

from gtk import *
from GDK import *

#import SendFile
from my_support import run_prog
from read_mailers import *

child_pid = None

class SendFile(GtkWindow):
    def __init__(self, path, mime_type="application/data"):
        GtkWindow.__init__(self)
        self.set_title(os.path.basename(path))
        self.path=path
        self.mime_type=mime_type

        vbox=GtkVBox(FALSE)
        self.add(vbox)

        hbox=GtkHBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=GtkLabel("To:")
        hbox.pack_start(label, FALSE, FALSE, 0)
        mail_to=GtkEntry()
        hbox.pack_start(mail_to, TRUE, TRUE, 0)
        self.mail_to=mail_to
                
        hbox=GtkHBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=GtkLabel("Subject:")
        hbox.pack_start(label, FALSE, FALSE, 0)
        subject=GtkEntry()
        hbox.pack_start(subject, TRUE, TRUE, 0)
        self.subject=subject
                
        hbox=GtkHBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        uuencode=GtkCheckButton("Encode as binary")
        hbox.pack_start(uuencode, FALSE, FALSE, 0)
        
        mime=self.mime_type.split('/')
        if mime[0] == 'text':
            uuencode.set_active(FALSE)
        else:
            uuencode.set_active(TRUE)

        self.uuencode=uuencode

        go=GtkButton("Send")
        hbox.pack_start(go, FALSE, FALSE, 0)

        vbox.show_all()
        go.connect('clicked', self.send_it)

    def send_it(self, widget):
        mailers=read_mailers_from(rox.choices.load('Mail', 'mailers.xml'))
        mailer=mailers[0]
        
        if self.uuencode.get_active():
            cmd="uuencode "+self.path+" "+os.path.basename(self.path)
            cmd=cmd+" | " +mailer.send_command(self.mail_to.get_text(),
                                               '/dev/stdin',
                                               self.subject.get_text())
        else:
            cmd=mailer.send_command(self.mail_to.get_text(), self.path,
                                     self.subject.get_text())

        child_pid=run_prog(cmd)

        self.destroy()
