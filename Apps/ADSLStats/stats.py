import os, sys

import findrox; findrox.version(2, 0, 2)
import rox, rox.choices, rox.options
import gobject

import urllib, urlparse, netrc
import HTMLParser
import struct, fcntl, termios

url='http://10.0.0.2/doc/adsl.htm'

class ROXURLopener(urllib.FancyURLopener):
    def prompt_user_passwd(self, host, realm):
        #print host
        #print realm
        try:
            pwds=netrc.netrc()
            ans=pwds.authenticators(host)
            if ans:
                return ans[0], ans[2]
            pwds=get_passwords()
            if pwds.has_key((host, realm)):
                user, password=pwds[host, realm]
            else:
                user=None
                password=None

            if user and password:
                return user, password
            ouser=user
            opass=password
            pd=PasswordWindow(host, realm, user, password)
            res=pd.run()
            if res==int(rox.g.RESPONSE_OK):
                user, password=pd.get_login()
                #print user, password
                if pd.get_save() and (user!=ouser or password!=opass):
                    add_password(pwds, host, realm, user, password)
                return user, password
        except:
            rox.report_exception()
        return None, None

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

def _data(node):
	"""Return all the text directly inside this DOM Node."""
	return ''.join([text.nodeValue for text in node.childNodes
			if text.nodeType == xml.dom.Node.TEXT_NODE])

def get_passwords():
    fname=rox.choices.load('kerofin.demon.co.uk', 'ADSLStats', 'passwords.xml')
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

def bytes_ready(s):
    try:
        d=struct.pack('i', 0)
        x=fcntl.ioctl(s, termios.FIONREAD, d)
        n=struct.unpack('i', x)[0]
        #print 'm=%d on %d %s' % (n, s, x) 
    except:
        n=-1
        print '%s: %s' % tuple(sys.exc_info()[:2])
    return n

class MyParser(HTMLParser.HTMLParser):
    def __init__(self, con):
        HTMLParser.HTMLParser.__init__(self)
        self.vars={}
        self.in_script=False
        self.con=con
        #self.lines=[]

    def update(self):
        try:
            while bytes_ready(self.con):
                line=self.con.readline()
                #print >>sys.stderr, len(line)
                self.feed(line)
            #print >>sys.stderr,'done'
        except:
            return False
        return True

    def getVars(self):
        #print self.lines
        return self.vars

    def getConSpeed(self):
        return self.vars['st_up_data_rate'], self.vars['st_dw_data_rate'], 

    def handle_starttag(self, tag, attrs):
        try:
            if tag=='script':
                for att in attrs:
                    #print att
                    if att[0]=='language' and att[1]=='javascript':
                        #print 'in'
                        self.in_script=True
        except:
            pass

    def handle_endtag(self, tag):
        if tag=='script':
            self.in_script=False

    def handle_data(self, data):
        if self.in_script:
            #self.lines.append(data)
            line=data.strip()
            if ';' in line:
                statement, tail=line.split(';')
                if statement[:4]=='var ':
                    var, val=statement[4:].split('=', 1)
                    var=var.strip()
                    val=val.strip()
                    if val[0]=='"' and val[-1]=='"':
                        val=val[1:-1]
                    self.vars[var]=val

def get():
    opener=ROXURLopener()
    con=opener.open(url)
    #headers=con.info()
    #print headers

    parser=MyParser(con)
    for line in con:
        parser.feed(line)
    parser.close()

    return parser.getVars()

def open():
    opener=ROXURLopener()
    con=opener.open(url)
    return MyParser(con)

if __name__=='__main__':
    stats=get()
    print '%s/%s' % (stats['st_dw_data_rate'], stats['st_up_data_rate'])
