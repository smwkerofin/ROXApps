import xml.sax
import xml.sax.saxutils
from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesImpl
from xml.dom.minidom import parse

class Mailer:
    def __init__(self, name, loc, send=None, read=None):
        self.name=name
        self.loc=loc
        self.send=send
        self.read=read
        if self.send==None:
            self.send="%(Command)s -s\"%(Subject)s\" %(To)s < %(File)s"

    def read_command(self):
        if self.read!=None:
            try:
                cmd=self.read % self.loc
            except:
                cmd=self.loc
            return cmd
        return self.loc

    def send_command(self, to, file, subject=None):
        if subject==None:
            subject="No subject"
        if self.send!=None:
            args={
                'Command': self.loc,
                'To': to,
                'File': file,
                'Subject': subject
                }
            try:
                cmd=self.send % args
            except:
                cmd="%s -s\"%s\" %s < %s" % (self.loc, subject, to, file)
            return cmd
        return "%s -s\"%s\" %s < %s" % (self.loc, subject, to, file)

    def old_save(self, file):
        file.write("<program name=\""+self.name+"\">\n")
        file.write("  <location file=\""+self.loc+"\" />\n")
        file.write("  <command type=\"send\">"+
                   xml.sax.saxutils.escape(self.send)+"</command>\n")
        if self.read!=None:
            file.write("  <command type=\"read\">"+
                       xml.sax.saxutils.escape(self.read)+"</command>\n")
        file.write("</program>\n")

    def save(self, gen):
        gen.startElement('program', AttributesImpl({'name': self.name}))
        gen.ignorableWhitespace('\n')
        
        gen.startElement('location', AttributesImpl({'file': self.loc}))
        gen.endElement('location')
        gen.ignorableWhitespace('\n')

        gen.startElement('command', AttributesImpl({'type': 'read'}))
        gen.characters(self.read)
        gen.endElement('command')
        gen.ignorableWhitespace('\n')

        gen.startElement('command', AttributesImpl({'type': 'send'}))
        gen.characters(self.send)
        gen.endElement('command')
        gen.ignorableWhitespace('\n')

        gen.endElement('program')
        gen.ignorableWhitespace('\n\n')

    def write_to(self, mailers, fname):
        file=open(fname, 'w')
        gen=XMLGenerator(file)

        gen.startDocument()
        no_attr=AttributesImpl({})
        gen.startElement('mailers', no_attr)
        gen.ignorableWhitespace('\n')
        for m in mailers:
            m.save(gen)

        gen.startElement('default', AttributesImpl({'name': mailers[0].name}))
        gen.endElement('default')
        gen.ignorableWhitespace('\n')
        gen.endElement('mailers')
        gen.ignorableWhitespace('\n')
        gen.endDocument()

    def read_from(self, fname):
        doc=parse(fname)

        progs=doc.getElementsByTagName("program")

        list=[]
        for p in progs:
            # print p
            name=p.getAttribute('name')
        
            for node in p.childNodes:
                if node.nodeType==node.ELEMENT_NODE:
                    if node.tagName=='location':
                        file=node.getAttribute('file')
                    elif node.tagName=='command':
                        type=node.getAttribute('type')
                        
                        tmp=""
                        for sub in node.childNodes:
                            if sub.nodeType==node.TEXT_NODE:
                                tmp=tmp+sub.data
                
                        if type=='read':
                            read=tmp
                        elif type=='send':
                            send=tmp

            mailer=Mailer(name, file, send, read)
            list.append(mailer)

        current=doc.getElementsByTagName("default");
        if len(current)>0:
            current=current[0];
            name=current.getAttribute('name')

            curmail=None
            for m in list:
                if m.name==name:
                    curmail=m
                    break
            if curmail!=None:
                i=list.index(curmail)
                if i>0:
                    if i==len(list)-1:
                        list=curmail+list[:i-1]
                    else:
                        list=curmail+list[:i-1]+list[i+1:]

        return list


