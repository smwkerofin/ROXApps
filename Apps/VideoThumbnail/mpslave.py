import os, time, errno
if __name__=='__main__':
    import findrox; findrox.version(2, 0, 4)
import rox
import gobject

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
        self.start_child(fname, *args)
            
    def start_child(self, fname, *args):
        cmd='mplayer -vo null -vf-clr -ao null -slave -quiet -vf screenshot '
        if args:
            a=' '.join(args)
            cmd+=a
        cmd+=' "'+fname+'"'

        #print cmd

        self.length=None
        self.last_line=None
        self.fname=fname

        stdin, stdout_and_err=os.popen4(cmd, 't', 0)
        self.child_out=stdin
        self.child_in=stdout_and_err

        self.load_file(None)

    def write_cmd(self, cmd):
        #print 'sending command:', cmd
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
        self.write_cmd('get_time_length')
        line=''
        while not line.startswith('ANS_LENGTH='):
            line=self.get_reply()
            if line is None:
                self.length=0
                return
            #print 'read', line
        self.last_line=line

        assert self.last_line.startswith('ANS_LENGTH=')
        l=self.last_line.strip()
        ignore, ans=l.split('=')
        self.length=float(ans)

    def get_time_pos(self):
        self.write_cmd('get_time_pos')
        line=''
        while not line.startswith('ANS_TIME_POSITION='):
            line=self.get_reply()
            if line is None:
                self.length=0
                return
            #print 'read', line
        self.last_line=line

        assert self.last_line.startswith('ANS_TIME_POSITION=')
        l=self.last_line.strip()
        ignore, ans=l.split('=')
        return float(ans)


    def quit(self):
        self.write_cmd('quit')
        self.read_all()
        self.child_out=None
        self.child_in=None

    def restart_child(self):
        self.read_all()
        self.start_child(self.fname)

    def seek_to(self, sec):
        self.write_cmd('seek %f 2' % sec)

    def screenshot(self):
        self.write_cmd('screenshot 0')
        res=self.get_reply()
        self.write_cmd('frame_step')
        res=self.get_reply()
        while not res.startswith('*** screenshot '):
            print 'res=',res
            res=self.get_reply()
        print 'res now', res
        if res.startswith('*** screenshot '):
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
        #print pos, p, abs(pos-p)
        if abs(pos-p)>0.25:
            self.load_file(self.fname)
            self.seek_to(0)
                
        return self.screenshot()

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

        
