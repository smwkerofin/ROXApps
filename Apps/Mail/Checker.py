import xml.sax
import xml.sax.saxutils
from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesImpl
from xml.dom.minidom import parse

class Checker:
    def __init__(self, cmd, int):
        self.cmd=cmd
        self.int=int
        self.time=0

    def getCommand(self):
        return self.cmd

    def getInterval(self):
        # print self.int
        return self.int

    def writeTo(self, gen):
        gen.startElement('check', AttributesImpl({'command': self.cmd,
                                                  'interval': self.int}))
        gen.endElement('check')
        gen.ignorableWhitespace('\n')

    def setTimeout(self, to):
        self.time=to

    def getTimeout(self):
        return self.time

def getCheckers(fname):
    doc=parse(fname)

    ch=doc.getElementsByTagName('check')
    if ch==None:
        return None

    checkers=[]
    for c in ch:
        cmd=c.getAttribute('command')
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

