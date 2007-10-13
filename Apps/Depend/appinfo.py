# $Id$

import os, sys, stat

from xml.dom import Node, minidom, XML_NAMESPACE

import findrox; findrox.version(1, 9, 12)
import rox.AppInfo

def data(node):
	"""Return all the text directly inside this DOM Node."""
	return ''.join([text.nodeValue for text in node.childNodes
			if text.nodeType == Node.TEXT_NODE])

class AppInfoDepends(rox.AppInfo.AppInfo):
    """Extend AppInfo class to handle Dependencies"""
    def __init__(self, app_dir, stream):
        while app_dir[-1]=='/':
            app_dir=app_dir[:-1]
        self.path=app_dir
        self.name=os.path.basename(app_dir)

        self.depends=None

	rox.AppInfo.AppInfo.__init__(self, stream)
	try:
                self.scan_doc()
	except:
                pass

    def scan_doc(self):
        nodes=self.findElements('Dependencies')
        for node in nodes:
		try:
			#print node
			self.scan_depends(node)
		except:
			pass
		
    def scan_depends(self, node):
        assert node.localName=='Dependencies'
        import depend
	#print node
	if self.depends is None:
		self.depends=[]
        for subnode in node.childNodes:
	    #print subnode
            if subnode.nodeType!=Node.ELEMENT_NODE:
                continue
	    #print subnode
            self.depends.append(depend.make_depend(subnode))
	    
    def getDependencies(self):
        return self.depends

    def getName(self):
        return self.name

    def getVersion(self):
        import depend
        #print self.about
        try:
            v=self.getAbout('Version')[1]
        except:
            return
        #print v
        if ' ' in v:
            v=v.split()[0]
        #print v
        return depend.versionTriple(v)

def read_app_info(app_dir_path, stream=None):
    if not stream:
	    try:
		    s=os.stat(app_dir_path)
	    except:
		    return
	    if stat.S_ISDIR(s.st_mode): 
		    stream=file(os.path.join(app_dir_path, 'AppInfo.xml'), 'r')
	    else:
		    stream=file(app_dir_path, 'r')
		    
    return AppInfoDepends(app_dir_path, stream)

if __name__=='__main__':
        
    for a in sys.argv[1:]:
        app=read_app_info(a)
        #print a
        #print app.name, app.version
        print app.getName(), app.getAbout('Version'), app.getVersion()
        for d in app.getDependencies():
            print ' %s (%s)' % (d, `d`)

