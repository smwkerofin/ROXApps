# $Id: fetch.py,v 1.2 2004/09/25 10:42:01 stephen Exp $

import os, sys
import time
import fcntl, termios, struct

import findrox; findrox.version(1, 9, 14)
import rox, rox.choices, rox.options
import gobject

import urllib, urlparse

import xml.dom, xml.dom.minidom

_=rox._

rox.setup_app_options('Fetch')

allow_pw_change=rox.options.Option('allow_pw_change', True)
wait_for=rox.options.Option('wait_for', 3)

rox.app_options.notify()

bsize=4096*2 # The bigger this number, the lighter the CPU load.  But we get
             # fewer updates to the GUI
stimeo=5*60

# This won't work in Python 2.2
import socket
try:
    socket.setdefaulttimeout(stimeo)
    set_to=True
except:
    set_to=False

def run_main():
    while rox.g.events_pending():
        rox.g.main_iteration()

# This won't work under Solaris (termios gets built without FIONREAD
# because it is defined in a different include file)
def bytes_ready(s):
    try:
        d=struct.pack('i', 0)
        x=fcntl.ioctl(s, termios.FIONREAD, d)
        n=struct.unpack('i', x)[0]
        #print 'm=%d on %d %s' % (n, s, x) 
    except:
        n=-1
        print 'm=%s: %s' % tuple(sys.exc_info()[:2])
    return n

class ROXURLopener(urllib.FancyURLopener):
    def prompt_user_passwd(self, host, realm):
        #print host
        #print realm
        try:
            pwds=get_passwords()
            if pwds.has_key((host, realm)):
                user, password=pwds[host, realm]
            else:
                user=None
                password=None

            if user and password and not allow_pw_change.int_value:
                return user, password
            ouser=user
            opass=password
            pd=PasswordWindow(host, realm, user, password)
            res=pd.run()
            if res==rox.g.RESPONSE_OK:
                user, password=pd.get_login()
                #print user, password
                if pd.get_save() and (user!=ouser or password!=opass):
                    add_password(pwds, host, realm, user, password)
                return user, password
        except:
            rox.report_exception()
        return None, None

class Fetcher:
    def __init__(self, url, fname):
        self.url=url
        self.target=fname

        self.start_time=None
        self.con=None
        self.bsize=bsize

        self.opener=ROXURLopener()
        self.open_server()
        self.run()
        
    def to_parent(self, type, message):
        print '%s=%s' % (type, message)
        sys.stdout.flush()

    def message(self, msg):
        self.to_parent('m', msg)
        
    def open_server(self):
        self.message(_('Connecting'))
        self.start_time=time.time()
        self.con=self.opener.open(self.url)
        self.message(_('Connected'))

        headers=self.con.info()
        #print headers
        if headers and headers.has_key('Content-Type'):
            self.to_parent('t', headers['Content-Type'])
        if headers.has_key("content-length"):
            self.to_parent('s', headers["Content-Length"])
            self.size=int(headers["Content-Length"])
        else:
            self.size=0
        #print self.con, dir(self.con)
        #print self.con.fp, dir(self.con.fp)
            
        self.outf=file(self.target, 'w')

        self.message(_('Downloading'))
        self.count=0
        self.report(0, self.size)
        #rox.g.timeout_add(100, self.update)
        #rox.g.idle_add(self.update)
        rox.g.input_add(self.con.fileno(), rox.g.gdk.INPUT_READ,
                        self.read_some)
        
        #print message
        #print dir(message)
        #self.finished(message)

    def close(self):
        #print 'close'
        try:
            self.con.close()
        except:
            pass
        try:
            self.outf.close()
        except:
            pass
        self.to_parent('c','')

    def read_some(self, source, condition, *unused):
        #print source, condition, unused

        nready=bytes_ready(self.con.fileno())
        #self.message('%d bytes ready' % nready)
        rsz=self.bsize
        if self.size>0:
            left=self.size-self.count
            if rsz>left:
                rsz=left
        if nready>-1 and rsz>nready:
            rsz=nready
        rsz=self.bsize

        if rsz>0:
            data=self.con.read(rsz)
            #self.message('%d bytes read' % len(data))
            self.count+=len(data)
            if data:
                self.report(self.count, self.size)
                self.outf.write(data)
            
                return True

        self.finished()
        return False

    def run(self):
        if self.con and self.outf:
            rox.g.mainloop()
        
    def report(self, nb, tsize):
        #print 'report', self, nb, tsize
        self.to_parent('n', str(nb))

    def finished(self):
        self.message(_('Done'))
        self.close()
        rox.g.main_quit()
        
class PasswordWindow(rox.Dialog):
    def __init__(self, host, realm, user=None, password=None):
        self.host=host
        self.realm=realm
        
        rox.Dialog.__init__(self)
        self.set_title(_('Password for %s' % host))
        self.add_button(rox.g.STOCK_CANCEL, rox.g.RESPONSE_CANCEL)
        self.add_button(rox.g.STOCK_OK, rox.g.RESPONSE_OK)
        self.connect('response', self.do_response)

        vbox=self.vbox

        table=rox.g.Table(4, 2)
        vbox.pack_start(table, padding=4)
        line=0
        
        l=rox.g.Label(_('Enter username and password for %s at %s') %
                      (realm, host))
        l.set_line_wrap(True)
        l.set_alignment(0., 0.5)
        table.attach(l, 0, 2, line, line+1, xpadding=2)

        line+=1
        l=rox.g.Label(_('<b>Username</b>'))
        l.set_use_markup(True)
        l.set_alignment(1., 0.5)
        table.attach(l, 0, 1, line, line+1, xpadding=2)
        self.user=rox.g.Entry(0)
        if user:
            self.user.set_text(user)
        table.attach(self.user, 1, 2, line, line+1, xpadding=2)

        line+=1
        l=rox.g.Label(_('<b>Password</b>'))
        l.set_use_markup(True)
        l.set_alignment(1., 0.5)
        table.attach(l, 0, 1, line, line+1, xpadding=2)
        self.password=rox.g.Entry(0)
        self.password.set_visibility(False)
        if password:
            self.password.set_text(password)
        table.attach(self.password, 1, 2, line, line+1, xpadding=2)

        line+=1
        self.can_save=rox.g.CheckButton(_('Remember for next time'))
        self.can_save.set_active(True)
        table.attach(self.can_save, 0, 2, line, line+1, xpadding=2)

        vbox.show_all()

    def do_response(self, *unused):
        self.hide()

    def get_login(self):
        return self.user.get_text(), self.password.get_text()

    def get_save(self):
        return self.can_save.get_active()

def _data(node):
	"""Return all the text directly inside this DOM Node."""
	return ''.join([text.nodeValue for text in node.childNodes
			if text.nodeType == xml.dom.Node.TEXT_NODE])

def get_passwords():
    fname=rox.choices.load('Fetch', 'passwords.xml')
    if not fname:
        return {}

    pwds={}
    doc=xml.dom.minidom.parse(fname)
    #print doc.getElementsByTagName('Entry')
    for entry in doc.getElementsByTagName('Entry'):
        host=entry.getAttribute('host')
        realm=entry.getAttribute('realm')

        node=entry.getElementsByTagName('User')[0]
        user=_data(node)
        node=entry.getElementsByTagName('Password')[0]
        password=_data(node)

        pwds[host, realm]=(user, password)

    return pwds

def save_passwords(pwds):
    fname=rox.choices.save('Fetch', 'passwords.xml')

    doc=xml.dom.minidom.Document()
    root=doc.createElement('Passwords')
    doc.appendChild(root)

    for key, value in pwds.iteritems():
        host, realm=key
        user, password=value

        node=doc.createElement('Entry')
        node.setAttribute('host', host)
        node.setAttribute('realm', realm)
        
        snode=doc.createElement('User')
        snode.appendChild(doc.createTextNode(user))
        node.appendChild(snode)
        snode=doc.createElement('Password')
        snode.appendChild(doc.createTextNode(password))
        node.appendChild(snode)
        
        root.appendChild(node)

    f=file(fname+'.tmp', 'w')
    doc.writexml(f)
    f.close()
    os.chmod(fname+'.tmp', 0600)
    os.rename(fname+'.tmp', fname)
        
def add_password(pwds, host, realm, user, password):
    pwds[host, realm]=(user, password)
    save_passwords(pwds)

def run(url, fname):
    fetcher=Fetcher(url, fname)

def main():
    try:
        run(sys.argv[1], sys.argv[2])
    except:
        ex, msg=sys.exc_info()[:2]
        print 'x=%s' % msg

if __name__=='__main__':
    main()
