import os
import sys

import netrc
import telnetlib
import time

class Modem(object):
    timeout=5
    prompt='home.gateway>'
    
    def __init__(self, host='10.0.0.1'):
        self.host=host
        self.connect(self.host)

    def connect(self, host=None):
        if not host:
            host=self.host
        nrc=netrc.netrc()
        login, account, password=nrc.authenticators(host)
        #print login, account, password

        self.tn=telnetlib.Telnet(host)
        self.login(login, password)

    def login(self, uname, password):
        self.tn.read_until('login: ')
        #print 'send login', uname
        self.tn.write(uname+'\n')
        if password:
            self.tn.read_until('Password: ')
            #print 'send password'
            self.tn.write(password+'\n')
        header=self.tn.read_until(self.prompt, self.timeout*4)
        self.header=map(lambda s: s.replace('\r',''), header.split('\n'))[:-1]

    def get_uptime(self):
        reply=self.command('wan adsl uptime')
        #print reply
        pref, data=reply[0].split(':')
        #print data
        el=data.strip().split()
        s=int(el[6])
        s+=int(el[4])*60
        s+=int(el[2])*60*60
        s+=int(el[2])*60*40*24
        return s

    def get_chandata(self):
        reply=self.command('wan adsl chandata')
        #print reply
        rx=None
        tx=None
        def get_kbps(line):
            var, val=line.split(':')
            v, unit=val.strip().split()
            return int(v)
        for line in reply:
            if line.startswith('near-end interleaved channel bit rate:'):
                rx=get_kbps(line)
            elif line.startswith('far-end fast channel bit rate:'):
                tx=get_kbps(line)
        return rx, tx
    
    def command(self, cmd):
        self.tn.write(cmd+'\n')
        data=self.tn.read_until(self.prompt, self.timeout)
        lines=map(lambda s: s.replace('\r',''), data.split('\n'))
        return lines[1:-1]

if __name__=='__main__':
    m=Modem()
    print m.get_uptime()
    print m.get_chandata()
    print m.header


