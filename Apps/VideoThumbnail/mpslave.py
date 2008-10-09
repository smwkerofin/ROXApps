import os, time, errno
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
        

class MPlayer(object):
    def __init__(self, fname, *args):
        self.debug=debug
        self.start_child(fname, *args)
            
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

        stdin, stdout_and_err=os.popen4(cmd, 't', 0)
        self.child_out=stdin
        self.child_in=stdout_and_err

        self.load_file(None)

    def write_cmd(self, cmd):
        if self.debug: print 'sending command:', cmd
        self.child_out.write(cmd+'\n')

    def get_reply(self):
        raw=self.child_in.readline()
        if raw=='':
            return None
        return raw.strip()

    def read_all(self):
        line=self.get_reply()
        while line is not None:
            #print 'read', line
            line=self.get_reply()

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
        self.get_time_length()

    def get_time_length(self):
        def match_length(s):
            return s.startswith('ANS_LENGTH=')
        
        reply=self.execute_command('get_time_length', match_length)
        if not reply:
            return

        assert reply.startswith('ANS_LENGTH=')
        #l=self.last_line.strip()
        ignore, ans=reply.split('=')
        #print ignore, ans
        self.length=float(ans)
        return self.length

    def get_time_pos(self):
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

    def restart_child(self):
        self.read_all()
        self.start_child(self.fname)

    def seek_to(self, sec):
        self.execute_command('seek %f 2' % sec)

    def screenshot(self):
        def match_screenshot(s):
            return s.startswith('sending VFCTRL_SCREENSHOT!')
        #self.debug=True
        res=self.execute_command('screenshot 0', match_screenshot)
        #res=self.get_reply()

        def match_taken(s):
            return s.startswith('*** screenshot ')
        res=self.execute_command('frame_step', match_taken)
        if self.debug: print 'res now', res
        if res and res.startswith('*** screenshot '):
            pre, fname, tail=res.split("'")
            #print fname
            fname=os.path.realpath(fname)
            wait_on_file(fname)
            pbuf=rox.g.gdk.pixbuf_new_from_file(fname)
            #print pbuf
            #os.remove(fname)
            return pbuf
        else:
            print res
            for i in range(8):
                print self.get_reply()
        #self.write_cmd('screenshot 1')
        #print self.get_reply()

    def make_frame(self):
        p=self.length*0.05
        #p=0
        if p>5*60:
            p=5*60
        try:
            self.seek_to(p)
        except IOError, exc:
            #print exc
            if exc.errno==errno.EPIPE:
                self.read_all()
                self.child_in=None
                self.child_out=None
                return
            raise
        pos=self.get_time_pos()
        fuzz=self.length*0.01
        if self.debug: print pos, p, abs(pos-p), fuzz
        if abs(pos-p)>fuzz:
            self.load_file(self.fname)
            self.seek_to(0)
                
        return self.screenshot()

    def execute_command(self, cmd, match_output=None):
        self.write_cmd(cmd)

        if match_output:
            return self.wait_for(match_output)

    def wait_for(self, match_fn):
        self.last_line=None
        if self.debug: print 'make task'
        rox.tasks.Task(self.read_task(match_fn))
        if self.debug: print 'call mainloop'
        rox.mainloop()
        if self.debug: print 'main loop done'
        return self.last_line

    def read_task(self, match_fn):
        rox.toplevel_ref()
        while True:
            yield rox.tasks.InputBlocker(self.child_in.fileno())
            self.last_line=self.get_reply()
            #if self.debug: print self.last_line
            if self.last_line is None:
                break
            if match_fn(self.last_line):
                break
        rox.toplevel_unref()

    def __del__(self):
        if self.child_out:
            self.child_out.write('quit\n')

def test():
    tdir='/home/stephen/tmp/vid'
    files=os.listdir(tdir)
    files=map(lambda x: os.path.join(tdir, x), os.listdir(tdir))

    mp=MPlayer(files[0])
    for f in files:
        mp.load_file(f)
        #print mp.fname, mp.length
        #print mp.make_frame()
    mp.quit()

if __name__=='__main__':
    test()

        
