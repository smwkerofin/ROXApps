# $Id: fetch.py,v 1.17 2007/02/18 11:42:21 stephen Exp $

import os, sys
import time
import fcntl, termios, struct

import findrox; findrox.version(2, 0, 4)
import rox, rox.choices, rox.options, rox.basedir
import gobject
try:
    from rox import xattr
except:
    xattr=None

import urllib, urlparse

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

stimeo=5*60

rox.setup_app_options('Fetch', site='kerofin.demon.co.uk')
from options import *
import passwords

rox.app_options.notify()

# This won't work in Python 2.2
import socket
try:
    socket.setdefaulttimeout(stimeo)
    set_to=True
except:
    set_to=False


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
            pwds=passwords.get_passwords()
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
            if res==int(rox.g.RESPONSE_OK):
                user, password=pd.get_login()
                #print user, password
                if pd.get_save() and (user!=ouser or password!=opass):
                    passwords.add_password(pwds, host, realm, user, password)
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
        self.bsize=block_size.int_value

        self.last_update=0
        self.update_delay=0.5
        
        self.opener=ROXURLopener()
        self.open_server()
        self.run()
        self.to_parent('c','')

    def to_parent(self, type, message):
        print '%s=%s' % (type, message)
        sys.stdout.flush()

    def message(self, msg):
        self.to_parent('m', msg)
        
    def open_server(self):
        self.message(_('Connecting to server'))
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
        gobject.io_add_watch(self.con.fileno(), gobject.IO_IN,
                        self.read_some)
        
        #print message
        #print dir(message)
        #self.finished(message)

    def close(self):
        #print 'close'
        self.to_parent('c','')
        try:
            self.con.close()
        except:
            pass
        try:
            self.outf.close()
        except:
            pass

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

        # override
        #rsz=self.bsize

        if rsz>0:
            #self.message('reading %d bytes' % rsz)
            data=self.con.read(rsz)
            #self.message('%d bytes read' % len(data))
            self.count+=len(data)
            if data:
                self.outf.write(data)
                self.report(self.count, self.size)

                #self.message('size=%d count=%d' % (self.size, self.count))
                if self.size==0 or self.count<self.size:
                    return True

        self.finished()
        return False

    def run(self):
        if self.con and self.outf:
            rox.toplevel_ref()
            rox.mainloop()
        
    def report(self, nb, tsize):
        #print 'report', self, nb, tsize
        now=time.time()
        if now-self.last_update>self.update_delay or tsize>0 and nb>=tsize:
            self.to_parent('n', str(nb))
            self.last_update=now

    def finished(self):
        #self.message(_('Done'))
        self.close()
        rox.toplevel_unref()

        if xattr:
            try:
                xattr.set(self.target, 'user.xdg.origin.url', self.url)
            except Exception, ex:
                print ex
        
class PasswordWindow(rox.Dialog):
    def __init__(self, host, realm, user=None, password=None):
        self.host=host
        self.realm=realm
        
        rox.Dialog.__init__(self)
        self.set_title(_('Password for %s') % host)
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

def main():
    try:
        Fetcher(sys.argv[1], sys.argv[2])
        print 'm=goodbye'
        time.sleep(1)
    except:
        ex, msg=sys.exc_info()[:2]
        print 'x=%s' % msg

if __name__=='__main__':
    main()
