import os

import xml.sax
import xml.sax.saxutils
from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesImpl
from xml.dom.minidom import parse

import gtk

class Checker:
    def __init__(self, cmd, int):
        self.cmd=cmd
        self.int=int
        self.time=0

    def getCommand(self):
        return self.cmd

    def setCommand(self, cmd):
        self.cmd=cmd

    def getInterval(self):
        # print self.int
        return self.int

    def setInterval(self, int):
        self.int=float(int)

    def writeTo(self, gen):
        gen.startElement('check', AttributesImpl({'command': self.cmd,
                                                  'interval': self.int}))
        gen.endElement('check')
        gen.ignorableWhitespace('\n')

    def setTimeout(self, to):
        self.time=to

    def getTimeout(self):
        return self.time

    def run(self):
        os.system(self.cmd)
        return 1

    def schedule(self):
        to=self.int*1000
        if self.time!=0:
            gtk.timeout_remove(self.time)
        def run_check(self):
            return self.run()
        self.time=gtk.timeout_add(int(to), run_check, self)

def getCheckers(fname):
    doc=parse(fname)

    ch=doc.getElementsByTagName('check')
    if ch==None:
        return None

    checkers=[]
    for c in ch:
        cmd=str(c.getAttribute('command'))
        interval=float(c.getAttribute('interval'))

        checkers.append(Checker(cmd, interval))

    return checkers

def writeCheckers(checkers, fname):
    file=open(fname, 'w')
    gen=XMLGenerator(file)

    gen.startDocument()

    for ch in checkers:
        ch.writeTo(gen)

    gen.endDocument()

