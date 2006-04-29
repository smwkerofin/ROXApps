#!/usr/bin/env python

import findrox
import rox
import rox.choices

import os.path
import os
import string

from rox import g
TRUE=g.TRUE
FALSE=g.FALSE

#import SendFile
from my_support import run_prog
from mailers import Mailer

child_pid = None

def check_child(pid):
    res=os.waitpid(pid, os.WNOHANG);
    #print res
    if res[0]>0:
        return 0
    return 1
    
class SendFile(rox.Window):
    def __init__(self, path, mime_type="application/data"):
        rox.Window.__init__(self)
        self.set_title(os.path.basename(path))
        self.path=path
        self.mime_type=mime_type

        vbox=g.VBox(FALSE)
        self.add(vbox)

        hbox=g.HBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=g.Label(_("To:"))
        hbox.pack_start(label, FALSE, FALSE, 0)
        mail_to=g.Entry()
        hbox.pack_start(mail_to, TRUE, TRUE, 0)
        self.mail_to=mail_to
                
        hbox=g.HBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        label=g.Label(_("Subject:"))
        hbox.pack_start(label, FALSE, FALSE, 0)
        subject=g.Entry()
        hbox.pack_start(subject, TRUE, TRUE, 0)
        self.subject=subject
                
        hbox=g.HBox(FALSE)
        vbox.pack_start(hbox, FALSE, FALSE, 0)

        uuencode=g.CheckButton(_("Encode as binary"))
        hbox.pack_start(uuencode, FALSE, FALSE, 0)
        
        mime=self.mime_type.split('/')
        if mime[0] == 'text':
            uuencode.set_active(FALSE)
        else:
            uuencode.set_active(TRUE)

        self.uuencode=uuencode

        go=g.Button(_("Send"))
        hbox.pack_start(go, FALSE, FALSE, 0)

        vbox.show_all()
        go.connect('clicked', self.send_it)

    def send_it(self, widget):
        fname=rox.choices.load('Mail', 'mailers.xml')
        if fname is None:
            mailer=Mailer('mailx', '/usr/bin/mailx')
        else:
            mailers=Mailer.read_from(Mailer('dummy', ''), fname)
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
        g.timeout_add(1000, check_child, child_pid);

        self.destroy()
