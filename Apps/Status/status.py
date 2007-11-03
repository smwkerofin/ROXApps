import os
import sys
import time

import finger

class Entry(object):
    def __init__(self, operator, date, message):
        self.operator=operator
        self.date=date
        self.message=message

    def get_message(self):
        return '\n'.join(self.message)

    @staticmethod
    def read(finger):
        #print finger
        fline=finger.readline()
        #print fline
        if fline is None:
            return

        fline=fline.strip()
        if not fline:
            return
        if fline[0]=='=' and fline[-1]=='=':
            return

        op, msg1=fline.split(None, 1)
        #print op, msg1
        #print op
        #print msg1

        sline=finger.readline()
        if not sline:
            return
        if not sline.strip():
            return

        #print `sline`
        #date=sline[:16].strip()
        try:
            tm, month, day, msg2=sline.strip().split(None, 3)
        except ValueError:
            tm, month, day=sline.strip().split()
            msg2=''
        if '.' in tm:
            tm=tm.replace('.', ':')
        #print tm, month, day, msg2
        date=tm+' '+month+' '+day
        #print `date`
        #msg2=sline[16:].strip()

        message=[msg1, msg2]

        blank=False
        while True:
            if blank and finger.peek()!=' ':
                break
            line=finger.readline()
            if line is None:
                break
            if not line.strip():
                blank=True
            elif line.strip()=='===========================':
                blank=True
            else:
                blank=False

            #print blank
            if not blank:
                message.append(line[16:].strip())

        now=time.localtime(time.time())
        #print `date`, now.tm_year
        try:
            t=time.strptime(date+' %d' % now.tm_year, '%H:%M %b %d %Y')
        except ValueError:
            t=now
        #print t

        return Entry(op, time.mktime(t), message)

    def __repr__(self):
        return '<Entry %s %s>' % (self.operator, self.date)

class Status(object):
    def __init__(self):
        self.entries={}

        self.update()

    def update(self):
        f=finger.Finger('status@gate')

        while True:
            line=f.readline()
            if line.startswith('Approximately 7 days history will be '):
                break

        discard=f.readline()

        newent=[]
        while True:
            ent=Entry.read(f)
            #print ent
            if not ent:
                break
            newent.append(ent)

        for ent in newent:
            if ent.date not in self.entries:
                self.entries[ent.date]=ent

    def get_entries(self, start=0):
        ans=[]
        keys=self.entries.keys()
        keys.sort()
        #print keys
        for k in keys:
            if k>start:
                ans.append(self.entries[k])
        return ans

def _test():
    stat=Status()
    print stat
    for e in stat.get_entries():
        print e, time.ctime(e.date)
        print e.get_message()

if __name__=='__main__':
    _test()

