# $Id: xattr.py,v 1.1.1.1 2005/08/13 11:27:43 stephen Exp $

import os, sys, errno

try:
    import ctypes
    try:
        object=ctypes.cdll.LoadLibrary('')
    except:
        object=ctypes.cdll.LoadLibrary('libc.so')
except:
    object=None

class NoXAttr(OSError):
    def __init__(self, path):
        self.OSError.__init__(errno.EOPNOTSUP, 'No xattr support on %s' % path,
                              path)
        

if object and hasattr(object, 'attropen'):
    #print 'attropen'

    try:
        _PC_XATTR_ENABLED=os.pathconf_names['PC_XATTR_ENABLED']
    except:
        _PC_XATTR_ENABLED=100  # Solaris 9
        
    try:
        _PC_XATTR_EXISTS=os.pathconf_names['PC_XATTR_EXISTS']
    except:
        _PC_XATTR_EXISTS=101  # Solaris 9

    def supported(path=None):
        if not path:
            return True

        return os.pathconf(path, _PC_XATTR_ENABLED)

    def present(path):
        return os.pathconf(path, _PC_XATTR_EXISTS)>0

    def get(path, attr):
        if os.pathconf(path, _PC_XATTR_EXISTS)<=0:
            return

        fd=object.attropen(path, attr, os.O_RDONLY, 0)
        if fd<=0:
            return

        v=''
        while True:
            buf=os.read(fd, 1024)
            if len(buf)<1:
                break
            v+=buf

        object.close(fd)

        return v

    def listx(path):
        return []

    def set(path, attr, value):
        fd=object.attropen(path, attr, os.O_WRONLY|os.O_CREAT, 0644)
        if fd<=0:
            raise NoXAttr(path)

        os.write(fd, value)
        object.close(fd)

    def delete(path, attr):
        pass

    name_invalid_chars='/\0'
    def name_valid(name):
        return name_invalid_chars not in name
    
    def binary_value_supported():
        return True

elif object and hasattr(object, 'getxattr'):
    #print 'getxattr'

    def supported(path=None):
        if not path:
            return False

        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        return True

    def present(path):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        buf=ctypes.c_buffer(1024)
        n=object.listxattr(path, ctypes.byref(buf), 1024)
        #print n, buf.value
        return n>0

    def get(path, attr):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        size=object.getxattr(path, attr, '', 0)
        #print size
        buf=ctypes.c_buffer(size+1)
        object.getxattr(path, attr, ctypes.byref(buf), size)
        return buf.value

    def listx(path):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        size=object.listxattr(path, None, 0)
        #print size
        if size<1:
            return []
        buf=ctypes.create_string_buffer(size)
        n=object.listxattr(path, ctypes.byref(buf), size)
        names=buf.raw[:-1].split('\0')
        return names

    def set(path, attr, value):
        #print 'set', path, attr, value
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        #print path, attr, value, len(value), 0
        res=object.setxattr(path, attr, value, len(value), 0)
        #print res
        if res==-1:
            raise EnvironmentError('Failed to set %s on %s' % (attr, path))
    
    def delete(path, attr):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)
        
        object.removexattr(path, attr)

    name_invalid_chars='\0'
    def name_valid(name):
        if '.' not in name or name[0]=='.':
            return False
        return name_invalid_chars not in name
    
    def binary_value_supported():
        return False

else:
    #print 'none'
    
    def supported(path=None):
        return False

    def present(path):
        return False

    def get(path, attr):
        return

    def listx(path):
        return []

    def set(path, attr, value):
        raise NoXAttr(path)

    def name_valid(name):
        return False

    def binary_value_supported():
        return False

if __name__=='__main__':
    if len(sys.argv)>1:
        path=sys.argv[1]
    else:
        path='/tmp'

    print path, supported(path)
    print path, present(path)
    print path, get(path, 'user.mime_type')
    print path, listx(path)
        
