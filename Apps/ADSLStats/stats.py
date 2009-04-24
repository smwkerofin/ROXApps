import os, sys

import findrox; findrox.version(2, 0, 2)
import rox, rox.choices, rox.options
import gobject

import urllib, urlparse, netrc
import HTMLParser
import struct, fcntl, termios

url='http://10.0.0.2/doc/adsl.htm'
tcpurl='http://10.0.0.2/doc/tcp.htm'
atmurl='http://10.0.0.2/doc/atm.htm'

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


class JSVarParser(MyParser):
    def __init__(self, con):
        MyParser.__init__(self, con)
        self.vars={}
        self.in_script=False
        
    def getVars(self):
        #print self.lines
        return self.vars

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

class BandwidthParser(JSVarParser):
    def __init__(self, con):
        JSVarParser.__init__(self, con)
        
    def getConSpeed(self):
        return (int(self.vars['st_up_data_rate']),
                int(self.vars['st_dw_data_rate']))

    def get_elapsed(self):
        d=int(self.vars['st_elased_time_days'])
        h=int(self.vars['st_elased_time_hours'])+d*24
        m=int(self.vars['st_elased_time_minutes'])+h*60
        s=int(self.vars['st_elased_time_seconds'])+m*60
        return s

class TCPParser(JSVarParser):
    def __init__(self, con):
        JSVarParser.__init__(self, con)

    def get_bytes(self):
        return int(self.vars['st_tcps_rcvbyte']), int(self.vars['st_tcps_sndbyte'])

class TableVarParser(MyParser):
    def __init__(self, con):
        MyParser.__init__(self, con)
        self.vars={}
        self.in_td=False
        self.vname=None
        
    def getVars(self):
        #print self.lines
        return self.vars

    def handle_starttag(self, tag, attrs):
        try:
            if tag=='td':
                self.in_td=True
        except:
            pass

    def handle_endtag(self, tag):
        if tag=='td':
            self.in_td=False

    def handle_data(self, data):
        if self.in_td:
            #self.lines.append(data)
            line=data.strip()
            if not self.vname:
                self.vname=line
            else:
                self.vars[self.vname]=data
                self.vname=None
            if ';' in line:
                statement, tail=line.split(';')

def get(turl=url, Parser=BandwidthParser):
    opener=ROXURLopener()
    con=opener.open(turl)
    #headers=con.info()
    #print headers

    parser=Parser(con)
    for line in con:
        parser.feed(line)
    parser.close()

    return parser


def _open(turl=url, Parser=BandwidthParser):
    opener=ROXURLopener()
    con=opener.open(turl)
    return Parser(con)

def open_adsl():
    return _open()

def open_tcp():
    return _open(tcpurl, TCPParser)

def open_atm():
    return _open(atmurl, TableVarParser)

def main():
    import time
    
    rep=len(sys.argv)>1
    if rep:
        delay=float(sys.argv[1])
    while True:
        if rep:
            print time.ctime(time.time())
        stats=get()
        #print stats.keys()
        tx, rx=stats.getConSpeed()
        print '%s/%s' % (rx, tx)
        print '%.2f hours elapsed' % (stats.get_elapsed()/3600.)
        parser=open_atm()
        time.sleep(0.1)
        parser.update()
        try:
            v=parser.getVars()
            tx=int(v['Tx Bytes'])
            rx=int(v['Rx Bytes'])
            print '%d/%d MB' % (rx>>20, tx>>20)
        except KeyError:
            print parser.getVars()
        if rep:
            time.sleep(delay)
        else:
            break
        

if __name__=='__main__':
    main()
    
