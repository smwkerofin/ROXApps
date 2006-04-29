# $Id: my_support.py,v 1.4 2005/06/04 10:30:43 stephen Exp $

import sys
import os.path
import os
import string
from stat import *
from string import find, lower, join
from socket import gethostbyaddr, gethostname

def run_prog(command):
    child_pid = os.fork()
    if child_pid == -1:
        report_error(_("fork() failed!"))
        return 127
    if child_pid == 0:
        try:
            error = os.system(command) != 0
            os._exit(error)
        except:
            pass
        os._exit(127)
    return child_pid

def extract_uris(data):
	lines = string.split(data, '\r\n')
	out = []
	for l in lines:
		if l and l[0] != '#':
			out.append(l)
	return out

def file_size(fname):
    try:
        s=os.stat(fname)
        size=s[ST_SIZE]
    except:
        size=0
    return size

def file_mtime(fname):
    try:
        s=os.stat(fname)
        size=s[ST_MTIME]
    except:
        size=0
    return size

def count_from(fname):
    # print 'count_from(%s)' % fname
    n=0
    f=open(fname, 'r')
    while 1:
        line=f.readline()
        if line is None or len(line)<1:
            break
        # print n, line
        if len(line)>4 and line[:5]=='From ':
            n=n+1
    return n

_host_name = None
def our_host_name():
	global _host_name
	if _host_name:
		return _host_name
	(host, alias, ips) = gethostbyaddr(gethostname())
	for name in [host] + alias:
		if find(name, '.') != -1:
			_host_name = name
			return name
	return name
	
def get_local_path(uri):
	"Convert uri to a local path and return, if possible. Otherwise,"
	"return None."
	host = our_host_name()

	if not uri:
		return None

	if uri[0] == '/':
		if uri[1] != '/':
			return uri	# A normal Unix pathname
		i = find(uri, '/', 2)
		if i == -1:
			return None	# //something
		if i == 2:
			return uri[2:]	# ///path
		remote_host = uri[2:i]
		if remote_host == host:
			return uri[i:]	# //localhost/path
		# //otherhost/path
	elif lower(uri[:5]) == 'file:':
		if uri[5:6] == '/':
			return get_local_path(uri[5:])
	elif uri[:2] == './' or uri[:3] == '../':
		return uri
	return None

