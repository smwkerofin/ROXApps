import os

import xml.sax
import xml.sax.saxutils
from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesImpl
from xml.dom.minidom import parse

import rox
g=rox.g

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
        gen.startElement('check', AttributesImpl({'command': str(self.cmd),
                                                  'interval': str(self.int)}))
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
            g.timeout_remove(self.time)
        self.time=g.timeout_add(int(to), self.run)

    def __del__(self):
        if self.time!=0:
            g.timeout_remove(self.time)

def getCheckers(fname):
    try:
        doc=parse(fname)
    except:
        rox.report_exception()
        return None

    ch=doc.getElementsByTagName('check')
    if ch==None:
        return None

    checkers=[]
    for c in ch:
        try:
            cmd=str(c.getAttribute('command'))
            interval=float(c.getAttribute('interval'))

            checkers.append(Checker(cmd, interval))
        except:
            pass

    return checkers

def writeCheckers(checkers, fname):
    try:
        file=open(fname, 'w')
        gen=XMLGenerator(file)

        gen.startDocument()

        gen.startElement('checkers', AttributesImpl({}))
        for ch in checkers:
            ch.writeTo(gen)
        gen.endElement('checkers')
        gen.ignorableWhitespace('\n')

        gen.endDocument()
    except:
        rox.report_exception()


