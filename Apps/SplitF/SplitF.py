from string import rfind, rindex
import os.path
import re
import stat
import os
import signal

from gtk import *
from GDK import *

from SaveBox import SaveBox
from support import *
import choices

split_formats = [
    # Extension, split, join
    (".spt", "splitf '%s'", "joinf '%s'")
    ]

def get_format(path, formats):
	for f in formats:
		ext = f[0]
		if ext == path[-len(ext):]:
			return f
	return None

def esc(text):
	"""Return text with \ and ' escaped"""
	return re.sub("'", "'\"'\"'", text)

def bg_system(command, out = None):
	"system(command), but still process GUI events while waiting..."
	"If 'out' is set then the child's stdout goes to that FD. 'out' is "
	"closed in the parent process."
	global child_pid

	(r, w) = os.pipe()
	
	child_pid = fork()
	if child_pid == -1:
		os.close(r)
		os.close(w)
		if out != None:
			os.close(out)
		report_error("fork() failed!")
		return 127
	if child_pid == 0:
		if out != None:
			os.dup2(out, 1)
		# Child
		os.setpgid(0, 0)	# Start a new process group
		try:
			os.close(r)
			error = os.system(command) != 0
			os.close(w)
			os._exit(error)
		except:
			pass
		os._exit(127)
	
	if out != None:
		os.close(out)
	os.close(w)

	done = []
	def cb(src, cond, done = done):
		done.append(1)
	input_add(r, INPUT_READ, cb)

	while not len(done):
		mainiteration()

	os.close(r)
	(pid, status) = waitpid(child_pid, 0)
	child_pid = None
	return status

def make_splitter(path):
	while path[-1:] == '/':
		path = path[:-1]
        f = get_format(path, split_formats)
        if f:
            window = JoinWindow(path, f)
        else:
            window = SplitWindow(path, split_formats[0])

        window.connect('destroy', mainquit)
        window.show()

class SplitF(SaveBox):
    def __init__(self, win, media, sub):
        SaveBox.__init__(self, win, media, sub);
        self.connect('destroy', self.destroyed)
	
    def destroyed(self, widget):
        global child_pid

        if child_pid:
            os.kill(-child_pid, signal.SIGTERM)
		
    def save_as(self, path):
		self.set_sensitive(FALSE);
		success = FALSE
		try:
			success = self.do_save(path)
		except:
			success = FALSE
			report_exception()
		self.set_sensitive(TRUE);
		return success
	
    def set_uri(self, uri):
		path = get_local_path(uri)
		if path:
			child('rox', '-x', path)
		pass

class SplitWindow(SplitF):
    def __init__(self, path, format):
        self.path=path
        new_ext = format[0]
        try:
            old_ext = path[rindex(path, "."):]
            self.uri = path[:-len(old_ext)] + new_ext
        except ValueError:
            self.uri = path + new_ext
        self.split = format[1]
        self.join = format[2]
        SplitF.__init__(self, self, 'application', 'x-split-file')
        
    def do_save(self, path):
        if bg_system(self.split % esc(self.path)):
            report_error("Split failed")
            return FALSE
        return TRUE

class JoinWindow(SplitF):
    def __init__(self, path, format):
        self.path=path
        ext = format[0]
        self.split = format[1]
        self.join = format[2]
        self.uri = path[:-len(ext)]
        SplitF.__init__(self, self, 'text', 'plain')
        
    def do_save(self, path):
        if bg_system(self.join % esc(self.path)):
            report_error("Join failed")
            return FALSE
        return TRUE
