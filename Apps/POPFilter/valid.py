# $Id$

"""Validate a mail target"""

import smtplib
import dbm
import pwd
import rfc822

aliases={}
try:
    afile=dbm.open('/etc/aliases', 'r')
    for k in afile.keys():
        aliases[k]=afile[k]
except:
    try:
        afile=file('/etc/aliases', 'r')
        #print afile
        for l in afile.readlines():
            l=l.strip()
            #print l, l.find(':')
            if l and l[0]!='#' and l.find(':')>=0:
                name, exp=l.split(':')
                aliases[name.lower().strip()]=exp.strip()
    except:
        pass

def validate_smtp(local):
    smtp=smtplib.SMTP('localhost')

    res, stri=smtp.verify(local)
    #print res, stri
    if res==250:
        return stri
    return None

def validate_aliases(local):
    if aliases.has_key(local):
        return '<%s@localhost>' % aliases[local]

def validate_passwd(local):
    #print 'validate_passwd',local
    try:
        p=pwd.getpwnam(local)
    except KeyError:
        return
    #print p

    return '%s <%s@localhost>' % (p[4], p[0])

def validate(local):
    local=local.lower()
    addr=None
    try:
        addr=validate_smtp(local)
    except:
        pass
    if not addr:
        try:
            addr=validate_passwd(local)
        except:
            pass
                
    if not addr:
        try:
            addr=validate_aliases(local)
        except:
            #import sys
            #print 'aliases failed',sys.exc_info()[1]
            pass

    if addr:
        rname, eaddr=rfc822.parseaddr(addr)
    
        return eaddr
    return None

if __name__=='__main__':
    import sys
    if len(sys.argv)>1:
        for n in sys.argv[1:]:
            print n, validate(n)
    else:
        print validate('postmaster')
