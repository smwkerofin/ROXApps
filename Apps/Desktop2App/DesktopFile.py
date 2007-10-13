# $Id$

"""Scan .desktop files and generate rox wrappers for programs"""

import string
import os
import sys

credit=sys.argv[0]+' 0.0.1'

def _copy(src, dest):
    """Copy a file in a single pass"""
    # print 'Copy %s to %s' % (src, dest)
    fin=file(src, 'r')
    dat=fin.read()
    fin.close()
    fout=file(dest, 'w')
    fout.write(dat)
    fout.close()

class DesktopEntry:
    def __init__(self, section, locale=None):
        self.section=section
        self.name=section.get('Name', locale)
        self.exc=section.get('Exec')
        self.comment=section.get('Comment', locale)
        self.terminal=section.getBoolean('Terminal')
        self.icon=section.get('Icon')
        g_pixmap_dir='share/pixmaps'
        self.ipath=[]
        for prefix in ('/usr/local/gnome', '/opt/gnome', '/usr/gnome'):
            self.ipath.append(os.path.join(prefix, g_pixmap_dir))
        kde_pixmap_dir='share/icons'
        for prefix in ('/usr/local/kde2', '/opt/kde2', '/usr/kde'):
            for col in ('hicolor', 'locolor'):
                for size in ('48x48', '64x64', '32x32', '22x22', '16x16'):
                    self.ipath.append(os.path.join(prefix, kde_pixmap_dir,
                                                   col, size, 'apps'))

        self.type=section.get('Type')
        self.no_display=section.getBoolean('NoDisplay', 0)
        self.path=section.get('Path')
        self.hidden=section.getBoolean('Hidden', 0)

    def setType(self, type):
        self.type=type
    def setNoDisplay(self, val):
        self.no_display=val

    def findIconFile(self):
        # print 'self.icon=%s' % self.icon
        base=self.icon
        if self.icon is None or len(self.icon)<1:
            return None
        if self.icon[0]=='/':
            if os.access(self.icon, os.R_OK):
                return self.icon
            base=os.path.basename(self.icon)

        for dir in self.ipath:
            path=os.path.join(dir, base)
            #print path, os.access(path, os.R_OK)
            if os.access(path, os.R_OK)==1:
                return path
            path=os.path.join(dir, base+'.png')
            #print path, os.access(path, os.R_OK)
            if os.access(path, os.R_OK)==1:
                return path
        return None

    def command(self, arg):
        cmd=self.exc
        if cmd is None:
            raise 'No valid command'
        while 1:
            per=string.find(cmd, '%')
            if per<0:
                break

            # Always a single file
            if cmd[per+1]=='f' or cmd[per+1]=='F':
                cmd=cmd[:per]+arg+cmd[per+2:]
            elif cmd[per+1]=='u' or cmd[per+1]=='U':
                cmd=cmd[:per]+'file://'+arg+cmd[per+2:]
            elif cmd[per+1]=='d' or cmd[per+1]=='D':
                cmd=cmd[:per]+'`dirname '+arg+'`'+cmd[per+2:]
            elif cmd[per+1]=='n' or cmd[per+1]=='n':
                cmd=cmd[:per]+'`basename '+arg+'`'+cmd[per+2:]
            elif cmd[per+1]=='i' or cmd[per+1]=='m':
                ifile=self.findIconFile()
                if ifile:
                    cmd=cmd[:per]+ifile+cmd[per+2:]
                else:
                    cmd=cmd[:per]+cmd[per+2:]
            elif cmd[per+1]=='c':
                if self.comment:
                    cmd=cmd[:per]+self.comment+cmd[per+2:]
                else:
                    cmd=cmd[:per]+cmd[per+2:]
            elif cmd[per+1]=='k':
                if self.section.fname:
                    cmd=cmd[:per]+self.section.fname+cmd[per+2:]
                else:
                    cmd=cmd[:per]+cmd[per+2:]
            elif cmd[per+1]=='k':
                dev=self.sections.get('Device')
                if dev:
                    cmd=cmd[:per]+dev+cmd[per+2:]
                else:
                    cmd=cmd[:per]+cmd[per+2:]
            else:
                cmd=cmd[:per]+cmd[per+2:]
                
        return cmd

    def writeAppRun(self, fname):
        if self.name is None:
            raise 'Name of app not given'
        out=file(fname, 'w')

        out.write('#!/bin/sh\n')
        out.write('# AppRun for %s created by %s\n' % (self.name,
                                                       credit))
        out.write('# %s\n\n' % self.comment)

        if self.path:
            out.write('cd %s\n' % self.path)
        cmd=self.command('$@')
        if self.terminal:
            opts=self.section.get('TerminalOptions')
            if opts:
                out.write('exec xterm %s -e %s\n' % (opts, cmd))
            else:
                out.write('exec xterm -e %s\n' % cmd)
        else:
            out.write('exec %s\n' % cmd)
        out.close()
        os.chmod(fname, 0755)

    def writeAppInfo(self, fname):
        if self.name is None:
            raise 'Name of app not given'
        out=file(fname, 'w')

        out.write('<?xml version="1.0"?>\n')
        out.write('<AppInfo>\n')
        if self.comment and len(self.comment)>0:
            out.write('  <Summary>%s</Summary>\n' % self.comment)
        out.write('  <About>\n')
        out.write('    <Purpose>Wrapper for %s</Purpose>\n' % self.name)
        out.write('    <Author>%s</Author>\n' % credit)
        out.write('  </About>\n')
        out.write('</AppInfo>\n')
        out.close()

    def makeAppDir(self, parent):
        if (self.name is None or self.exc is None or
            self.hidden or self.type!='Application' or self.no_display):
            return None
        if os.access(parent, os.W_OK)!=1:
            raise 'Cannot write to '+parent

        if self.type:
            dir=os.path.join(parent, self.type, self.name)
        else:
            dir=os.path.join(parent, self.name)
        if os.access(dir, os.F_OK)!=1:
            os.makedirs(dir)
        if os.access(dir, os.W_OK)!=1:
            raise 'Cannot write to '+dir
        self.writeAppRun(os.path.join(dir, 'AppRun'))
        self.writeAppInfo(os.path.join(dir, 'AppInfo.xml'))
        ifile=self.findIconFile()
        if ifile:
            #print 'Copy %s to .DirIcon' % ifile
            try:
                _copy(ifile, os.path.join(dir, '.DirIcon'))
            except:
                type, value=sys.exc_info()[:2]
                print 'Failed to copy icon: %s %s' % (type, value)
                print 'Was copying %s to %s' % (ifile, os.path.join(dir, '.DirIcon'))
        return dir
        
class Section:
    def __init__(self, name, fname=None):
        self.name=name
        self.vars={}
        self.fname=fname

    def get(self, vname, locale=None):
        try:
            var=self.vars[vname]
        except KeyError:
            return None

        if locale is None:
            try:
                return var['C']
            except:
                return None

        try:
            val=var[locale]
        except KeyError:
            t=string.find(locale, '_')
            if t>0:
                locale=locale[:t]

                try:
                    val=var[locale]
                    return val
                except KeyError:
                    pass
            try:
                val=var['C']
            except KeyError:
                val=None

        return val

    def add(self, vname, val, locale='C'):
        try:
            var=self.vars[vname]
        except KeyError:
            var={}
            self.vars[vname]=var

        var[locale]=val

    def getVars(self, locale=None):
        res={}
        for var in self.vars.keys():
            res[var]=self.get(var, locale)
        return res

    def getBoolean(self, name, defval=0, locale=None):
        val=self.get(name, locale)
        if val is None:
            return defval
        if val=='true' or val=='1':
            return 1
        return 0

    def getDesktopEntry(self, locale=None):
        if self.name!='Desktop Entry':
            return None

        ent=DesktopEntry(self, locale)

        return ent

class DesktopFile:
    def __init__(self, fname=None):
        self.sections=[]
        if fname:
            self.read(fname)

    def clear(self):
        self.sections=[]
    
    def addSection(self, name, fname=None):
        sect=Section(name, fname)
        self.sections.append(sect)
        return sect

    def read(self, fname):
        fl=file(fname, 'r')
        if fl is None:
            return None

        cur=self.addSection('')
        while 1:
            line=fl.readline()
            if line is None or len(line)<1:
                break
            line=string.strip(line)
            if len(line)<1:
                continue
            if line[0]=='#':
                continue
            if line[0]=='[' and line[-1]==']':
                cur=self.addSection(line[1:-1], fname)
            else:
                pos=string.find(line, '=')
                if pos<1:
                    continue
                
                var=string.rstrip(line[:pos])
                val=string.lstrip(line[pos+1:])
                lang=string.find(var, '[')
                if lang>0:
                    if lang==1:
                        continue
                    term=string.find(var, '[', lang+1)
                    if term<0:
                        continue
                    locale=var[lang+1:term]
                    var=var[:lang]
                    cur.add(var, val, locale)
                else:
                    cur.add(var, val)

    def getSection(self, section):
        for sect in self.sections:
            if sect.name==section:
                return sect
        return None
    
    def get(self, section, var, locale='C'):
        sect=self.getSection(section)
        if sect is None:
            return None

        return sect.get(var, locale)

    def getSections(self, named=None):
        if named is None:
            return self.sections

        res=[]
        for sect in self.sections:
            if named==sect.name:
                res.append(sect)
        return res

    def merge(self, other):
        for sect in other.sections:
            self.sections.append(sect)

    def makeAppDirs(self, dir, locale=None):
        if os.access(dir, os.F_OK)!=1:
            os.makedirs(dir)
        for sect in self.sections:
            ent=sect.getDesktopEntry(locale)
            if ent:
                # print ent.type, ent.name
                # print 'Icon:', ent.findIconFile()
                dir=ent.makeAppDir(dir)
                if dir: print dir
            

if __name__=='__main__':
    try:
        locale=os.environ['LANG']
    except KeyError:
        locale=None
    if len(sys.argv)<2:
        raise 'Need one or more desktop files as arguments'
    dir='tmp'
    start=1
    if sys.argv[1]=='-t':
        dir=sys.argv[2]
        start=3
    for dfile in sys.argv[start:]:
        desk=DesktopFile(dfile)
        desk.makeAppDirs(dir, locale)
            
