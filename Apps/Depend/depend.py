# $Id: depend.py,v 1.3 2004/05/12 18:46:49 stephen Exp $

"""depend"""

import os, sys

from xml.dom import Node, minidom

OK=0
VERSION=1
MISSING=2
stat_msgs=(_('Ok'), _('Old version'), _('Missing'))

import appinfo

try:
    libdirpath=os.environ['LIBDIRPATH']
    libdirpaths=libdirpath.split(':')
except:
    libdirpaths=[os.environ['HOME']+'/lib', '/usr/local/lib', '/usr/lib']

try:
    libpath=os.environ['LD_LIBRARY_PATH']
    libpaths=libpath.split(':')
except:
    libpaths=[os.environ['HOME']+'/lib', '/usr/local/lib']
libpaths.append('/usr/lib')

try:
    f=file('/etc/ld.so.conf', 'r')
    for line in f.readlines():
        libpaths.append(line.strip())
except:
    pass

class Dependency:
    def __init__(self, type, name, version=None, uri=None):
        self.type=type
        self.name=name
        self.version=version
        self.uri=uri

        self.status=MISSING
        self.where=None
        self.actual_version=None

    def check(self):
        return self.status

    def info(self):
        state=self.check()
        return self.type, self.name, self.version, state, self.where, self.actual_version, self.uri

    def __str__(self):
        return '%s %s %s %s' % (self.type, self.name, self.version,
                                stat_msgs[self.check()])
    def getVersion(self):
        return versionTriple(self.version)

    def getLocation(self):
        return self.where

    def getLink(self):
        return self.uri

class FileDependency(Dependency):
    def __init__(self, type, paths, name, version=None, uri=None):
        Dependency.__init__(self, type, name, version, uri)
        self.paths=paths

    def check(self):
        leaf=self.get_leaf(self.version)
        for dir in self.paths:
            path=os.path.join(dir, leaf)
            #print 'try',path
            if os.access(path, os.R_OK) and self.is_ok(path):
                self.where=path
                return self.version_check(path)

        leaf=self.get_leaf(None)
        for dir in self.paths:
            path=os.path.join(dir, leaf)
            #print 'try', path
            if os.access(path, os.R_OK) and self.is_ok(path):
                self.where=path
                return self.version_check(path)

        self.status=MISSING
        return self.status

    def get_leaf(self, version):
        return self.name

    def is_ok(self, path):
        return True

    def version_check(self, path):
        """Default is not to check version"""
        self.status=OK
        self.actual_version=None
        return self.status
    
def data(node):
	"""Return all the text directly inside this DOM Node."""
	return ''.join([text.nodeValue for text in node.childNodes
			if text.nodeType == Node.TEXT_NODE])
def versionTriple(verstr):
    if verstr is None:
        return
    vers=verstr.split('.')
    while len(vers)<3:
        vers.append('0')
    return map(int, vers)
    
def make_depend(node):
    global _readers
    type=node.localName
    name=None
    version=None
    uri=None
    #print node.localName
    for subnode in node.childNodes:
        #print subnode
        if subnode.nodeType!=Node.ELEMENT_NODE:
            continue
        #print subnode, subnode.localName
        if subnode.localName=='Name':
            name=data(subnode)
        elif subnode.localName=='NeedsVersion':
            version=data(subnode)
        elif subnode.localName=='Link':
            uri=data(subnode)

    if name is None:
        raise _('No name defined for dependency')
    try:
        return _readers[type](name, version, uri)
    except:
        print '%s %s' % sys.exc_info()[:2]
        return Dependency(type, name, version)
    

class AppDirDependency(Dependency):
    def __init__(self, name, version=None, uri=None):
        Dependency.__init__(self, 'AppDir', name, version, uri)

    def check(self):
        rvers=self.getVersion()
        paths=map(lambda p: os.path.join(p, self.name), os.environ['PATH'].split(':')+libdirpaths)
        for path in paths:
            #print path
            if os.access(path, os.R_OK):
                #print 1
                try:
                    #print path, appinfo.read_app_info
                    app=appinfo.read_app_info(path)
                    #print app
                except:
                    continue
                if rvers:
                    try:
                        vers=app.getVersion()
                    except:
                        self.status=VERSION
                        self.where=path
                        #return self.status
                    #print vers, rvers
                    if not vers or not (vers[0]>=rvers[0] and
                                        vers[1]>=rvers[1] and
                                        vers[2]>=rvers[2]):
                        self.status=VERSION
                    else:
                        self.status=OK
                    if vers:
                        #print vers
                        #print '%d.%d.%d' % tuple([1, 9, 13])
                        self.actual_version='%d.%d.%d' % tuple(vers)
                else:
                    self.status=OK
                self.where=path
                if self.status==OK:
                    break
        
        return self.status

class PythonDependency(Dependency):
    def __init__(self, name, version=None, uri=None):
        Dependency.__init__(self, 'Python', name, version, uri)

    def check(self):
        try:
            #eval('import '+self.name)
            __import__(self.name)
            self.status=OK
        except:
            #print 'Reading module: %s %s' % sys.exc_info()[:2]
            self.status=MISSING
        return self.status

class LibraryDependency(FileDependency):
    """Dependancy on a single shared library, in the usual places"""
    def __init__(self, name, version=None, uri=None):
        FileDependency.__init__(self, 'Library', libpaths, name, version, uri)

    def get_leaf(self, version):
        if version:
            leaf='lib%s.so.%s' % (self.name, version)
        else:
            leaf='lib%s.so' % self.name
        return leaf
    
class ProgramDependency(FileDependency):
    """Dependancy on a single program, in the usual places"""
    def __init__(self, name, version=None, uri=None):
        FileDependency.__init__(self, 'Program',
                                os.environ['PATH'].split(':'), name,
                                version, uri)

    def is_ok(self, path):
        if os.access(path, os.X_OK):
            return True
        return False
    
class PackageDependency(Dependency):
    """Dependancy on a pkg-config package"""
    def __init__(self, name, version=None, uri=None):
        Dependency.__init__(self, 'Package', name, version, uri)
        
    def check(self):
        if self.version:
            cmd='pkg-config %s --atleast-version %s' % (self.name, self.version)
        else:
            cmd='pkg-config --exists %s' % self.name

        res=os.system(cmd)
        #print cmd, res
        if res==0:
            self.status=OK
            self.set_version()
            self.set_location()
            return self.status

        if self.version:
            res=os.system('pkg-config --exists %s' % self.name)
            #print res
            if res==0:
                self.status=VERSION
                self.set_version()
                self.set_location()
                return self.status

        self.status=MISSING
        return self.status

    def set_version(self):
        f=os.popen('pkg-config --modversion %s' % self.name, 'r')
        lines=f.readlines()
        f.close()
        self.actual_version=lines[0].strip()

    def set_location(self):
        f=os.popen('pkg-config --variable=prefix %s' % self.name, 'r')
        lines=f.readlines()
        f.close()
        loc=lines[0].strip()
        if loc:
            self.where=loc

_readers={
    'AppDir': AppDirDependency,
    'Program': ProgramDependency,
    'Python': PythonDependency,
    'Library': LibraryDependency,
    'Package': PackageDependency
    }

if __name__=='__main__':
    for a in sys.argv[1:]:
        app=AppDir(a)
        #print a
        #print app.name, app.version
        print app.getName(), app.getVersion()
        for d in app.getDependencies():
            print ' %s (%s)' % (d, `d`)
