import os, time, errno
import subprocess
if __name__=='__main__':
    import findrox; findrox.version(2, 0, 4)
import rox, rox.tasks
import gobject

debug=os.environ.get('VIDTHUMB_DEBUG', 0)

def wait_on_file(fname):
    l=0
    while True:
        try:
            s=os.stat(fname)
        except OSError, exc:
            if exc.errno!=errno.ENOENT:
                raise
            time.sleep(0.2)
            continue

        if s.st_size>0 and s.st_size==l:
            break
        l=s.st_size

class InputTOBlocker(rox.tasks.InputBlocker, rox.tasks.TimeoutBlocker):
    def __init__(self, stream, timeout):
        self.timed_out=False

        rox.tasks.InputBlocker.__init__(self, stream)
        rox.tasks.TimeoutBlocker.__init__(self, timeout)
        rox.toplevel_unref() # lose the ref from TimeoutBlocker

    def _timeout(self):
        print self, 'timed out'
        #print rox._toplevel_windows
        #print rox._in_mainloops
        self.timed_out=True
        self.trigger()
        try:
            rox.g.main_quit()
        except RuntimeError, err:
            # Can happen if we have already left the main loop because
            # we are reporting an exception elsewhere.
            print err

class TimedOut(Exception):
    def __init__(self, waiting_for):
        self.what=waiting_for
    def __str__(self):
        return 'Timed out waiting for %s' % self.what

class MatchProperty(object):
    def __init__(self, propname):
        self.propname=propname

    def __call__(self, s):
        return s.startswith('ANS_'+self.propname)

class MPlayer(object):
    def __init__(self, fname, *args):
        self.debug=debug
        self.timed_out=False
        self.frame_width=None
        self.frame_height=None
        self.start_child(fname, *args)

        if self.debug: print self, 'debug enebled'
            
    def start_child(self, fname, *args):
        cmd='mplayer -vo null -vf-clr -ao null -slave -quiet -vf screenshot '
        if args:
            a=' '.join(args)
            cmd+=a
        cmd+=' "'+fname+'"'

        if self.debug: print cmd

        self.length=None
        self.last_line=None
        self.fname=fname

        #stdin, stdout_and_err=os.popen4(cmd, 't', 0)
        #self.child_out=stdin
        #self.child_in=stdout_and_err

        self.child=subprocess.Popen(cmd, shell=True,
                                    stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE,
                                    close_fds=True)
        self.child_out=self.child.stdin
        self.child_in=self.child.stdout
        self.child_error=self.child.stderr

        self.load_file(None)

    def write_cmd(self, cmd):
        if self.debug: print 'SENDING COMMAND:', cmd
        self.child_out.write(cmd+'\n')

    def get_reply(self, tout=15):
        def any_line(s):
            if self.debug: print 'check', s
            return True
        self.wait_for(any_line, tout, False)
        return self.last_line

    def _get_reply(self):
        raw=self.child_in.readline()
        if not raw or raw=='':
            return None
        return raw.strip()

    def read_all(self, tout=15):
        if self.debug: print 'read_all'
        line=self.get_reply(tout)
        while line is not None:
            if self.debug: print 'read', line
            line=self.get_reply(tout)

    def load_file(self, fname):
        if not self.child_in or not self.child_out:
            self.start_child(fname)
            self.fname=fname
        elif fname:
            self.write_cmd('loadfile "%s" 0' % fname)
            self.fname=fname
        self.length=None
        self.write_cmd('pause')
        self.write_cmd('loop -1')
        #self.write_cmd('get_property filename')
        #self.write_cmd('get_property video_codec')
        #self.write_cmd('get_property width')
        #self.write_cmd('get_property height')
        self.get_frame_size()
        self.get_time_length()

    def get_property(self, propname):
        ans=self.execute_command('get_property '+propname,
                                    MatchProperty(propname))
        if not ans:
            return

        if '=' in ans:
            return ans.split('=', 1)[1]

    def get_frame_size(self):
        v=self.get_property('width')
        if v:
            try:
                self.frame_width=int(v)
            except ValueError:
                pass
        v=self.get_property('height')
        if v:
            try:
                self.frame_height=int(v)
            except ValueError:
                pass

    def get_time_length(self):
        if self.debug: print 'get_time_length', rox._toplevel_windows
        def match_length(s):
            return s.startswith('ANS_LENGTH=')
        
        reply=self.execute_command('get_time_length', match_length)
        if not reply:
            return
        if self.debug: print 'reply is', reply
        if not reply.startswith('ANS_LENGTH='):
            return

        #l=self.last_line.strip()
        ignore, ans=reply.split('=')
        #print ignore, ans
        self.length=float(ans)
        return self.length

    def get_time_pos(self):
        if self.debug: print 'get_time_pos', rox._toplevel_windows
        def match_pos(s):
            return s.startswith('ANS_TIME_POSITION=')
        
        reply=self.execute_command('get_time_pos', match_pos)
        if not reply:
            if self.debug: print 'failed to get time position'
            return
        if self.debug:print reply

        assert reply.startswith('ANS_TIME_POSITION=')
        #l=self.last_line.strip()
        ignore, ans=reply.split('=')
        return float(ans)


    def quit(self):
        self.execute_command('quit', lambda s: s.startswith('Exit '))
        self.child_out=None
        self.child_in=None
        self.child.wait()

    def restart_child(self):
        self.read_all()
        self.start_child(self.fname)

    def check_child(self, restart=True):
        self.child.poll()
        if self.debug: print 'child returncode', self.child.returncode
        died=self.child.returncode is not None
        if died:
            self.child.wait()
        if died and self.debug:
            for line in self.child_error.readlines():
                print 'CHILD STDERR:', line.strip()
        if self.child.returncode<0:
            signum=-self.child.returncode
            if signum==signal.SIGSEGV:
                rox.croak('mplayer killed by segmentation violation')
            elif self.debug:
                print 'mplayer killed by signal', signum
        if died and restart:
            self.restart_child()
        return not died

    def seek_to(self, sec):
        self.execute_command('seek %f 2' % sec)

    def screenshot(self):
        def match_screenshot(s):
            print 'match_screenshot', repr(s)
            return s.startswith('sending VFCTRL_SCREENSHOT!')
        #self.debug=True
        res=self.execute_command('screenshot 0', match_screenshot)
        #res=self.get_reply()

        def match_taken(s):
            print 'match_taken', repr(s)
            if self.debug: print s, '*** screenshot '
            return s.startswith('*** screenshot ')
        res=self.execute_command('frame_step', match_taken)
        if self.debug: print 'res now', res
        if res and res.startswith('*** screenshot '):
            pre, fname, tail=res.split("'")
            fname=os.path.realpath(fname)
            if debug: print fname
            wait_on_file(fname)
            pbuf=rox.g.gdk.pixbuf_new_from_file(fname)
            if debug: print pbuf

            if pbuf and pbuf.get_height()>pbuf.get_width():
                if debug: print 'rescale'
                pbuf=pbuf.scale_simple(pbuf.get_width()*2,
                                       pbuf.get_height(),
                                       rox.g.gdk.INTERP_NEAREST)
            #os.remove(fname)
            return pbuf
        else:
            if self.debug: print res
            for i in range(8):
                l=self.get_reply(1)
                if self.debug: print l
                if l is None:
                    break
        #self.write_cmd('screenshot 1')
        #print self.get_reply()

    def make_frame(self, frac_length=None):
        if self.debug: print 'in make_frame'
        self.check_child()
        if frac_length is None:
            p=self.length*0.05
            #p=0
            if p>5*60:
                p=5*60
        else:
            p=self.length*frac_length
                
        try:
            self.seek_to(p)
        except IOError, exc:
            if self.debug: print exc
            if exc.errno==errno.EPIPE:
                self.read_all()
                self.child_in=None
                self.child_out=None
                return
            raise
        pos=self.get_time_pos()
        
        fuzz=self.length*0.05
        if self.debug: print pos, p, abs(pos-p), fuzz
        if abs(pos-p)>fuzz:
            self.load_file(self.fname)
            self.seek_to(0)

        good_ratio=True
        if not self.frame_width or not self.frame_height or self.frame_width<self.frame_height:
            good_ratio=False
        if not good_ratio:
            self.write_cmd('switch_ratio 1.333333')
                
        return self.screenshot()

    def execute_command(self, cmd, match_output=None):
        self.write_cmd(cmd)

        if self.debug: print rox._toplevel_windows
        if match_output:
            if self.debug: print 'sent', cmd, 'match', match_output
            return self.wait_for(match_output)

    def wait_for(self, match_fn, tout=15, raise_on_timed_out=True):
        if self.debug: print rox._toplevel_windows
        self.timed_out=False
        self.last_line=None
        if self.debug: print 'make task'
        t=rox.tasks.Task(self.read_task(match_fn, tout, raise_on_timed_out))
        if self.debug: print rox._toplevel_windows
        if self.debug: print rox._in_mainloops
        if self.debug: print 'call mainloop', t
        rox.g.main()
        if self.debug: print rox._toplevel_windows
        if self.debug: print rox._in_mainloops
        if self.debug: print 'main loop done'
        if self.timed_out:
            return None
        return self.last_line

    def read_task(self, match_fn, tout, raise_on_timed_out):
        if self.debug: print 'read_task', match_fn
        #if self.debug: print rox._toplevel_windows
        #if self.debug: print rox._in_mainloops
        rox.toplevel_ref()
        while True:
            block=InputTOBlocker(self.child_in.fileno(), tout)
            yield block
            if self.debug: print 'yielded', block
            if block.timed_out:
                if self.debug: print block, 'timed out'
                self.timed_out=True
                if self.debug and raise_on_timed_out:
                    rox.toplevel_unref()
                    raise TimedOut('reply to command')
                break
            self.last_line=self._get_reply()
            if self.debug: print 'last line', self.last_line
            if self.last_line is None:
                if self.debug: print 'EOF from child?'
                self.check_child(False)
                break
            if match_fn(self.last_line):
                if self.debug: print 'match'
                break
            if self.debug: print 'no match'
        rox.toplevel_unref()
        # TODO check this
        if self.debug:  print rox._toplevel_windows
        if self.debug:  print rox._in_mainloops
        rox.g.main_quit()

    def __del__(self):
        if self.child_out:
            try:
                self.child_out.write('quit\n')
            except IOError:
                pass

def test():
    tdir='/home/stephen/tmp/vid'
    files=os.listdir(tdir)
    files=map(lambda x: os.path.join(tdir, x), os.listdir(tdir))

    mp=MPlayer(files[0])
    for f in files:
        mp.load_file(f)
        print mp.fname, mp.length
        pbuf=mp.make_frame()
        print pbuf.get_width(), pbuf.get_height()
        #print mp.make_frame()
    mp.quit()

if __name__=='__main__':
    test()

        
