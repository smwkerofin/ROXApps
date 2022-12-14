# $Id: xattr.py,v 1.2 2006/08/03 11:00:54 stephen Exp $

import os, sys, errno

try:
    import ctypes
    try:
        libc=ctypes.cdll.LoadLibrary('')
    except:
        libc=ctypes.cdll.LoadLibrary('libc.so')

except:
    libc=None

class NoXAttr(OSError):
    def __init__(self, path):
        OSError.__init__(self, errno.EOPNOTSUP, 'No xattr support',path)
        

if libc and hasattr(libc, 'attropen'):
    #print 'attropen'
    libc_errno=ctypes.c_int.in_dll(libc, 'errno')
    def _get_errno():
        return libc_errno.value
    def _error_check(res, path):
        if res<0:
            raise OSError(_get_errno(), os.strerror(_get_errno()), path)

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

        fd=libc.attropen(path, attr, os.O_RDONLY, 0)
        if fd<0:
            return

        v=''
        while True:
            buf=os.read(fd, 1024)
            if len(buf)<1:
                break
            v+=buf

        libc.close(fd)

        return v

    def listx(path):
        if os.pathconf(path, _PC_XATTR_EXISTS)<=0:
            return []

        fd=libc.attropen(path, '.', os.O_RDONLY, 0)
        if fd<0:
            return []

        odir=os.getcwd()
        os.fchdir(fd)
        attrs=os.listdir('.')
        os.chdir(odir)
        libc.close(fd)

        return attrs

    def set(path, attr, value):
        fd=libc.attropen(path, attr, os.O_WRONLY|os.O_CREAT, 0644)
        _error_check(fd, path)

        res=os.write(fd, value)
        libc.close(fd)
        _error_check(res, path)

    def delete(path, attr):
        fd=libc.attropen(path, '.', os.O_RDONLY, 0)
        _error_check(fd, path)

        res=libc.unlinkat(fd, attr, 0)
        libc.close(fd)
        _error_check(res, path)

    name_invalid_chars='/\0'
    def name_valid(name):
        return name_invalid_chars not in name
    
    def binary_value_supported():
        return True

elif libc and hasattr(libc, 'getxattr'):
    #print 'getxattr'
    #print hasattr(libc, 'errno')
    if hasattr(libc, '__errno_location'):
        libc.__errno_location.restype=ctypes.c_int # _p
        errno_loc=libc.__errno_location()
        #print errno_loc
        libc_errno=ctypes.c_int.from_address(errno_loc)
        #print libc_errno, libc_errno.value
        
    elif hasattr(libc, 'errno'):
        libc_errno=ctypes.c_int.in_dll(lib, 'errno')

    else:
        libc_errno=ctypes.c_int(errno.EOPNOTSUP)

    def _get_errno():
        return libc_errno.value
    def _error_check(res, path):
        if res<0:
            raise OSError(_get_errno(), os.strerror(_get_errno()), path)

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
        n=libc.listxattr(path, ctypes.byref(buf), 1024)
        #print n, buf.value
        return n>0

    def get(path, attr):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        size=libc.getxattr(path, attr, '', 0)
        if size<0:
            #aise OSError(libc_errno.value, os.strerror(libc_errno.value),
            #              path
            return
        #print size
        buf=ctypes.c_buffer(size+1)
        libc.getxattr(path, attr, ctypes.byref(buf), size)
        return buf.value

    def listx(path):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        size=libc.listxattr(path, None, 0)
        #print size
        if size<1:
            return []
        buf=ctypes.create_string_buffer(size)
        n=libc.listxattr(path, ctypes.byref(buf), size)
        names=buf.raw[:-1].split('\0')
        return names

    def set(path, attr, value):
        #print 'set', path, attr, value
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)

        #print path, attr, value, len(value), 0
        res=libc.setxattr(path, attr, value, len(value), 0)
        _error_check(res, path)

    
    def delete(path, attr):
        if not os.access(path, os.F_OK):
            raise OSError(errno.ENOENT, 'No such file or directory', path)
        
        res=libc.removexattr(path, attr)
        _error_check(res, path)

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

    set(path, 'user.test', 'this is a test')
    print path, listx(path)
    print path, get(path, 'user.test')
    delete(path, 'user.test')
    print path, listx(path)
