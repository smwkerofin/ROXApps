# $Id$

import sys
import os
import re

import findrox
import rox
import rox.choices
import rox.options
import rox.OptionsBox

import StringIO
import poplib
import rfc822

import valid
import killfile

g=rox.g

ID=0
FROM=1
TO=2
SIZE=3
SUBJECT=4
ACTION=5
SIZE_VALUE=6
FROM_COLOUR=7
TO_COLOUR=8
SIZE_COLOUR=9
ACTION_COLOUR=10
SUBJECT_COLOUR=11

COLOUR_NORMAL='white'
COLOUR_KILL='red'
COLOUR_LEAVE='green'

ACTION_LEAVE='Leave'
ACTION_KILL='Kill'

interval=250

# user options
rox.setup_app_options('POPFilter')

mailhost=rox.options.Option('MailHost', 'localhost')
port=rox.options.Option('POP3port', 110)
try:
    name=os.getlogin()
except:
    try:
        name=os.getenv('LOGNAME')
    except:
        name=''
account=rox.options.Option('Account', name)
passwd=rox.options.Option('Password', '')

validate=rox.options.Option('ValidateName', 1)
maxsize=rox.options.Option('MaxSize', 0)

rox.app_options.notify()

def fmt_size(size):
    #print size, (4L<<30), size>(4L<<30)
    if size>(4L<<30):
        return '%4d GB' % (size>>30)
    elif size>(4<<20):
        return '%4d MB' % (size>>20)
    elif size>(4<<10):
        return '%4d KB' % (size>>10)
    return '%4d  B' % size

def find_addrs(mes):
    fr=''
    to=''
    subj=''

    t='\n'.join(mes)+'\n'
    m=rfc822.Message(StringIO.StringIO(t), 0)
    try:
        ignore, fr=rfc822.parseaddr(m['from'])
    except:
        pass
    try:
        subj=m['subject']
    except:
        pass
    try:
        # The problem is for multi-drop sites, such as my demon account.
        # Which of the possibly multiple To-addresses is the one we should
        # be looking at?
        addrs=rfc822.AddressList(m['to'])
        for name, addr in addrs:
            local, host=addr.split('@')
            #print local, host
            if host==rox.our_host_name():
                to=local
                break
        if not to:
            for name, addr in addrs:
                local, host=addr.split('@')
                #print local, os.getlogin()
                if local==os.getlogin():
                    to=local
                    break
    except:
        print `sys.exc_info()[1]`
        pass

    #print fr, to, subj
    return fr, to, subj

class MyOptions(rox.OptionsBox.OptionsBox):
    """Extension to the OptionsBox to provide a secretentry widget"""
    def build_secretentry(self, node, label, option):
        "<secretentry name='...' label='...' char='*'>Tooltip</sectryentry>"
        try:
            ch=node.getAttribute('char')
            if len(ch)>=1:
                ch=ch[0]
            else:
                ch=u'\0'
        except:
            ch='*'
        box = g.HBox(g.FALSE, 4)
        entry = g.Entry()
        entry.set_visibility(g.FALSE)
        entry.set_invisible_char(ch)

        if label:
            label_wid = g.Label(label)
            label_wid.set_alignment(1.0, 0.5)
            box.pack_start(label_wid, g.FALSE, g.TRUE, 0)
            box.pack_start(entry, g.TRUE, g.TRUE, 0)
        else:
            box = None

        self.may_add_tip(entry, node)

        entry.connect('changed', lambda e: self.check_widget(option))
        
        def get():
            return entry.get_chars(0, -1)
        def set():
            entry.set_text(option.value)
        self.handlers[option] = (get, set)

        return [box or entry]


class MainWin(rox.Window):
    def __init__(self, host=None, account=None, passwd=None, port=110):
        rox.Window.__init__(self)
        self.set_default_size(600, 400)

        self.vbox=g.VBox()
        self.add(self.vbox)

        hbox=g.HBox()
        self.vbox.pack_start(hbox, False, False, 0)

        self.stat_line=g.Label('Initializing...')
        hbox.pack_start(self.stat_line)

        swin = g.ScrolledWindow()
        swin.set_border_width(4)
        swin.set_shadow_type(g.SHADOW_IN)
        self.vbox.pack_start(swin, True, True, 0)

        self.model = g.ListStore(str, str, str, str, str, str, int, str, str,
                                 str, str, str)
        view = g.TreeView(self.model)
        self.view = view
        swin.add(view)

        self.view.get_selection().set_mode(g.SELECTION_MULTIPLE)
        
        cell = g.CellRendererText()
        column = g.TreeViewColumn('ID', cell, text = ID)
        view.append_column(column)
        column.set_sort_column_id(ID)

        cell = g.CellRendererText()
        column = g.TreeViewColumn('From', cell, text = FROM,
                                  background=FROM_COLOUR)
        view.append_column(column)
        column.set_sort_column_id(FROM)

        cell = g.CellRendererText()
        column = g.TreeViewColumn('To', cell, text = TO,
                                  background=TO_COLOUR)
        view.append_column(column)
        column.set_sort_column_id(TO)

        cell = g.CellRendererText()
        column = g.TreeViewColumn('Subject', cell, text = SUBJECT,
                                  background=SUBJECT_COLOUR)
        view.append_column(column)
        column.set_sort_column_id(SUBJECT)
        
        cell = g.CellRendererText()
        column = g.TreeViewColumn('Size', cell, text = SIZE,
                                  background=SIZE_COLOUR)
        view.append_column(column)
        column.set_sort_column_id(SIZE)
        
        cell = g.CellRendererText()
        column = g.TreeViewColumn('Action', cell, text = ACTION,
                                  background=ACTION_COLOUR)
        view.append_column(column)
        column.set_sort_column_id(ACTION)

        hbox=g.HBox()
        self.vbox.pack_start(hbox, False, False, 0)

        but=rox.ButtonMixed(g.STOCK_DELETE, 'Kill selected')
        but.connect('clicked', self.kill_selected)
        hbox.pack_start(but)
        self.kill_button=but
        but=rox.ButtonMixed(g.STOCK_APPLY, 'Leave selected')
        but.connect('clicked', self.leave_selected)
        hbox.pack_start(but)
        self.leave_button=but

        hbox=g.HBox()
        self.vbox.pack_start(hbox, False, False, 0)

        but=rox.ButtonMixed(g.STOCK_PROPERTIES, 'Options')
        def edit_opts(but, self):
            self.edit_options()
        but.connect('clicked', edit_opts, self)
        hbox.pack_start(but)

        but=g.Button('Kill file')
        but.connect('clicked', self.edit_killfile)
        hbox.pack_start(but)

        but=g.Button('Local users')
        but.connect('clicked', self.edit_users)
        hbox.pack_start(but)

        hbox=g.HBox()
        self.vbox.pack_start(hbox, False, False, 0)

        but=rox.ButtonMixed(g.STOCK_REFRESH, 'Reconnect')
        but.connect('clicked', self.connect_to_really)
        hbox.pack_start(but)
        
        hbox=g.HBox()
        self.vbox.pack_start(hbox, False, False, 0)

        but=rox.ButtonMixed(g.STOCK_EXECUTE, 'Process actions')
        but.connect('clicked', self.process_mailbox)
        hbox.pack_start(but)

        self.vbox.show_all()

        try:
            self.from_kill=killfile.load('POPFilter')
        except:
            self.from_kill=[]
        self.to_kill=[]
        self.to_no_kill=[]
        self.to_validate=validate.int_value
        self.subject_kill=[]
        self.max_size=maxsize.int_value*1024

        self.mail=None
        self.nmail=0
        self.index=-1

        self.set_title('%s - %s' % (host, account))

        self.host=host
        self.port=port
        self.account=account
        self.passwd=passwd

        if host and account and passwd:
            g.timeout_add(interval, self.connect_to_really)

        rox.app_options.add_notify(self.options_changed)

        self.options_box=None
        self.killfile_edit=None
        self.users_edit=None

    def clear_list(self):
        self.model.clear()

    def kill_selected(self, ignore=None):
        selection=self.view.get_selection()

        def do_kill(model, path, iter, self):
            model.set(iter, ACTION, ACTION_KILL, ACTION_COLOUR, COLOUR_KILL)

        selection.selected_foreach(do_kill, self)

    def leave_selected(self, ignore=None):
        selection=self.view.get_selection()

        def do_leave(model, path, iter, self):
            model.set(iter, ACTION, ACTION_LEAVE, ACTION_COLOUR, COLOUR_LEAVE)

        selection.selected_foreach(do_leave, self)

    def kill_by_from(self, fr):
        #return fr in self.from_kill
        for rexp in self.from_kill:
            if rexp.match(fr):
                return 1

    def kill_by_subject(self, fr):
        for rexp in self.subject_kill:
            if rexp.match(fr):
                return 1

    def kill_by_to(self, to):
        for rexp in self.to_no_kill:
            if rexp.match(to):
                return 0
        for rexp in self.to_kill:
            if rexp.match(to):
                return 1
        if self.to_validate and not valid.validate(to):
            return 1
        return 0

    def kill_by_size(self, size):
        if self.max_size==0:
            return 0
        if size>self.max_size:
            print size, self.max_size
            return 1
        return 0

    def add_line(self, id, fr, to, subject, size):
        action=ACTION_LEAVE
        fr_colour=COLOUR_NORMAL
        to_colour=COLOUR_NORMAL
        size_colour=COLOUR_NORMAL
        subject_colour=COLOUR_NORMAL

        if self.kill_by_from(fr):
            action=ACTION_KILL
            fr_colour=COLOUR_KILL
        if self.kill_by_to(to):
            action=ACTION_KILL
            to_colour=COLOUR_KILL
        if self.kill_by_size(size):
            action=ACTION_KILL
            size_colour=COLOUR_KILL
        if self.kill_by_subject(subject):
            action=ACTION_KILL
            subject_colour=COLOUR_KILL

        if action==ACTION_KILL:
            action_colour=COLOUR_KILL
        else:
            action_colour=COLOUR_LEAVE

        iter=self.model.append()
        self.model.set(iter, ID, str(id), FROM, fr, TO, to,
                       SIZE, fmt_size(size), SUBJECT, subject, 
                       ACTION, action, SIZE_VALUE, size, 
                       FROM_COLOUR, fr_colour, TO_COLOUR, to_colour,
                       SUBJECT_COLOUR, subject_colour,
                       SIZE_COLOUR, size_colour, ACTION_COLOUR, action_colour)

    def rules_changed(self):
        iter=self.model.get_iter_first()
        selection=self.view.get_selection()
        while iter:
            action=ACTION_LEAVE
            fr_colour=COLOUR_NORMAL
            to_colour=COLOUR_NORMAL
            size_colour=COLOUR_NORMAL
            subject_colour=COLOUR_NORMAL

            if self.kill_by_from(self.model.get_value(iter, FROM)):
                fr_colour=COLOUR_KILL
                action=ACTION_KILL
            if self.kill_by_to(self.model.get_value(iter, TO)):
                to_colour=COLOUR_KILL
                action=ACTION_KILL
            if self.kill_by_size(self.model.get_value(iter, SIZE_VALUE)):
                size_colour=COLOUR_KILL
                action=ACTION_KILL
            if self.kill_by_subject(self.model.get_value(iter, SUBJECT)):
                subject_colour=COLOUR_KILL
                action=ACTION_KILL

            if action==ACTION_KILL:
                action_colour=COLOUR_KILL
            else:
                action_colour=COLOUR_LEAVE

            self.model.set(iter, ACTION, action,
                           FROM_COLOUR, fr_colour, TO_COLOUR, to_colour,
                           SIZE_COLOUR, size_colour,
                           SUBJECT_COLOUR, subject_colour,
                           ACTION_COLOUR, action_colour)

            iter=self.model.iter_next(iter)
            
    def add_to_killfile(self, kill):
        self.from_kill.append(re.compile(kill, re.IGNORECASE))

    def set_killfile(self, kills):
        self.from_kill=kills
        self.rules_changed()

    def get_killfile(self):
        return self.from_kill

    def status(self, txt):
        self.stat_line.set_label(txt)
        while g.events_pending():
            g.main_iteration()

    def connect_to(self, host, account, passwd, port=110):
        self.set_title('%s - %s' % (host, account))

        self.host=host
        self.port=port
        self.account=account
        self.passwd=passwd

        self.connect_to_really()

    def connect_to_really(self, ignored=None):
        self.status('Connecting to %s...' % self.host)
        self.clear_list()
        try:
            self.mail=poplib.POP3(self.host, self.port)
            self.status(self.mail.getwelcome())
            self.mail.user(self.account)
            self.status('Logged in as %s, sending password' % self.account)
            self.mail.pass_(self.passwd)
        except:
            ex=sys.exc_info()[1]
            self.status('Failed to connect: %s' % ex)
            rox.report_exception()
            return

        try:
            self.nmail, size=self.mail.stat()
            self.index=0
        except:
            ex=sys.exc_info()[1]
            self.status('Failed to read mailbox: %s' % ex)
            rox.report_exception()
            self.end_connect(0)
            return
        self.status('%d messages, %d bytes' % (self.nmail, size))

        #print self.nmail
        if self.nmail>0:
            g.timeout_add(interval, self.read_msg)
        else:
            self.status('No mail at %s for %s' % (self.host, self.account))

    def check_connect(self):
        if not self.mail:
            self.status('Not connected')
            return 0
        try:
            self.mail.noop()
        except:
            rox.alert('Lost connection: %s' % sys.exc_info()[1])
            self.mail=None
            self.model.clear()
            self.status('Lost connection')
            return 0
        return 1
            
    def read_msg(self):
        if not self.check_connect():
            return
        try:
            self.status('Checking message %d' % (self.index+1))
            resp, msg, octets=self.mail.top(self.index+1, 0)
        except:
            ex=sys.exc_info()[1]
            self.status('Failed to read message %d: %s' % (self.index+1, ex))
            rox.report_exception()
            self.end_connect(0)
            return

        #print resp
        #print octets

        id=self.index+1
        fr, to, subj=find_addrs(msg)

        try:
            resp, env, sz=self.mail._longcmd('*ENV %d' % id)
            if resp[:3]=='+OK':
                #print env
                #print 'To: ',env[3]
                to, account=env[3].split('@')
        except:
            # *ENV not supported
            if not to:
                if self.account.find('@')>0:
                    to, account=self.account.split('@')

        self.add_line(id, fr, to, subj, octets)

        self.index+=1
        if self.index<self.nmail:
            return 1
        self.status(self.mail.getwelcome())

    def end_connect(self, delete=1):
        if not self.mail:
            return

        try:
            if not delete:
                self.mail.rset()
            self.mail.quit()
        except:
            pass
        self.mail=None
        self.model.clear()
        self.status('Not connected')

    def process_mailbox(self, ignored):
        if not self.check_connect():
            return

        ndel=0
        iter=self.model.get_iter_first()
        while iter:
            action=self.model.get_value(iter, ACTION)
            id=self.model.get_value(iter, ID)

            if action==ACTION_KILL:
                try:
                    self.mail.dele(int(id))
                    ndel+=1
                except:
                    rox.report_exception()
            
            iter=self.model.iter_next(iter)

        if ndel<1:
            self.status('None deleted.')
            return
            
        if rox.confirm('Delete mails?', g.STOCK_DELETE):
            self.end_connect()
            self.status('%d deleted.  Disconnected' % ndel)
            g.timeout_add(5000, self.connect_to_really)
        else:
            self.mail.rset()
            self.status('None deleted.')
    
    def edit_options(self):
        if self.options_box:
            self.options_box.present()
            return

        options_file = os.path.join(rox.app_dir, 'Options.xml')

        try:
            self.options_box=MyOptions(rox.app_options, options_file)
            self.options_box.connect('destroy', self.options_closed)
            self.options_box.open()
        except:
            rox.report_exception()
            self.options_box = None

    def options_closed(self, widget):
        assert self.options_box == widget
        self.options_box = None
                
    def options_changed(self):
        ch=0
        if validate.has_changed:
            self.to_validate=validate.int_value
            ch=1
        if maxsize.has_changed:
            self.size=maxsize.int_value*1024
            ch=1

        if ch:
            self.rules_changed()

        if mailhost.has_changed:
            self.host=mailhost.value
        if port.has_changed:
            self.port=port.value
        if account.has_changed:
            self.account=account.value
        if passwd.has_changed:
            self.passwd=passwd.value

    def edit_killfile(self, widget):
        if self.killfile_edit:
            self.killfile_edit.present()
            return

        try:
            self.killfile_edit=killfile.KillFileEdit(self)
            self.killfile_edit.connect('destroy', self.killfile_closed)
            self.killfile_edit.show()
        except:
            rox.report_exception()
            self.killfile_edit = None
           
    def killfile_closed(self, widget):
        assert self.killfile_edit == widget
        self.killfile_edit = None
                
    def edit_users(self, widget):
        if self.users_edit:
            self.users_edit.present()
            return

        try:
            self.users_edit=users.UsersEdit(self)
            self.users_edit.connect('destroy', self.users_closed)
            self.users_edit.show()
        except:
            rox.report_exception()
            self.users_edit = None
           
    def users_closed(self, widget):
        assert self.users_edit == widget
        self.users_edit = None
                
        
if __name__=='__main__':
    try:
        if rox.choices.load('POPFilter', 'Options.xml') is None:
            # User has not configured us yet, disclaimers
            import rox.filer
            rox.filer.show_file(os.path.join(rox.app_dir, 'Help', 'DISCLAIMER'))
        w=MainWin(mailhost.value, account.value, passwd.value, port.int_value)
        w.show()
        rox.mainloop()
    except:
        rox.report_exception()

