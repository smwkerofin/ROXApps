import os, sys, time
import dbus, dbus.glib

# DBus setup
service_name='uk.co.demon.kerofin.DownloadManager'
interface_name=service_name+'.Control'
new_interface_name=service_name+'.Control2'
object_path='/DownloadManager'

try:
    new_dbus=dbus.version>=(0, 40, 0)
except:
    new_dbus=False

defdelay=5
inc=5

if hasattr(dbus, 'set_default_main_loop'):
    dbus.set_default_main_loop(dbus.glib.DBusGMainLoop())

class DMException(Exception):
    """Generic DownloadManager exception"""
    pass

class DMNotRunning(DMException):
    """DownloadManager is not running"""
    pass

class DMNoAnswer(DMException):
    """DownloadManager did not answer us"""

class Manager:
    """This class represents a connection to the DownloadManager."""
    def __init__(self, service=service_name, object=object_path):
        """Create and instance of the class.  Do not call this directly,
        use the connect() call below"""
        if new_dbus:
            self.bus=dbus.SessionBus()
        else:
            self.bus=dbus.Bus(dbus.Bus.TYPE_SESSION)

        if new_dbus:
            self.object=self.bus.get_object(service, object)
            self.iface=dbus.Interface(self.object, interface_name)
            self.iface2=None
            
        else:
            tservice=self.bus.get_service(service)
            self.object=tservice.get_object(object, interface_name)
            self.iface=self.object
            try:
                self.iface2=tservice.get_object(object, new_interface_name)
            except:
                self.iface2=None

        try:
            if new_dbus:
                running=self.iface.Ping()
            else:
                running=self.object.Ping()
        except:
            running=False
            
        if not running:
            raise DMNotRunning

        if self.iface2:
            self.client_id=self.iface2.RequestID('dclient')
        else:
            self.client_id=None

    def getQueueSize(self):
        """Return number of clients in the queue waiting to start.  If
        we are waiting then this will include us."""
        try:
            return self.iface.QueueSize()
        except:
            pass
        raise DMNoAnswer

    def acquire(self, host, fname, block=True, delay=defdelay):
        """Attempt to start.
        host - name of host we will be downloading from
        fname - name of resource we will be downloading
        block - if True (the default) wait until we are given the go ahead
                (use False if a GUI program and will do own polling)
        delay - delay in seconds to wait between polls if block is True
        If true is returned then the client can start, false means to wait.
        If an exception is raised then DownloadManager is not responding and
        the client should start anyway."""
        check=True
        while check:
            try:
                if self.iface2:
                    ok=self.iface2.CanIStartByID(self.client_id, host, fname)
                else:
                    ok=self.iface.CanIStart(host, fname)
            except:
                raise DMNoAnswer

            if ok:
                return True

            if not block:
                check=False

            if check:
                time.sleep(delay)

        return False

    def update(self, size, total=-1):
        """Tell DownloadManager we have downloaded some more.  Call
        periodically to ensure we keep our slot.  Don't call if no data
        received just to maintain the slot, that defeats the purpose.
        size - number of bytes downloaded so far
        total - total number of bytes to download (optional)"""
        try:
            if self.iface2:
                self.iface2.UpdateByUI(self.client_id, size, total)
            else:
                self.iface.Update(size, total)
        except:
            pass

    def done(self):
        """Download has finished sucessfully"""
        try:
            if self.iface2:
                self.iface2.DoneByID(self.client_id)
            else:
                self.iface.Done()
        except:
            pass

    def cancel(self, why):
        """Download was cancelled, either by user intervention or some error.
        why - string describing the reason the download was cancelled"""
        try:
            if self.iface2:
                self.iface2.CancelByID(id, why)
            else:
                self.iface.Cancel(why)
        except:
            #self.report_exception('Cancel (%s)' % why)
            pass

    def register(self, fn):
        """Register a function to be called when a slot becomes available
        fn - function to call, passed: interface, signal_name, service, path, message
        When this is called, you can then call acquire to try and claim
        this slot."""
        self.iface.connect_to_signal('slot_available', fn)
        print 'call', fn, 'for', 'slot_available', 'on', self.iface
        # Doesn't appear to work...

    def showOptions(self):
        """Show the download manager's options window."""
        self.iface.ShowOptions()

    def getStats(self):
        """Return a list of strings describing each active client."""
        return self.iface.GetStats()

    def getActive(self):
        """Return number of downloads in progress."""
        return self.iface.GetActive()

    def getWaiting(self):
        """Return number of clients waiting."""
        return self.iface.GetWaiting()

    def showWindow(self, show=True):
        """If show==True then show the download window, otherwise
        hide it."""
        self.iface.ShowWindow(show)

    def ping(self):
        """Return True if the server is still responding."""
        try:
            ok=self.iface.Ping()
        except:
            ok=False

        return ok

    def report_exception(self, what):
        obj=sys.exc_info()[1]
        print >>sys.stderr, 'Exception in %s: %s' % (what, obj)

def connect(start=False):
    """Make connection to DownloadManager.
    start - if True and the DownloadManager is not running attempt to start it
            (defaults to False)
    Returns a Manager object representing the connect.
    Can raise a DMException."""
    try:
        man=Manager()
    except DMNotRunning:
        if not start:
            raise

        # Start DownloadManager here
        import rox, time
        path=os.getenv('APPDIRPATH', os.getenv('PATH'))
        path=path.split(':')
        for p in path:
            x=os.path.join(p, 'DownloadManager')
            if rox.isappdir(x):
                #print >> sys.stderr, x
                os.spawnl(os.P_NOWAIT, os.path.join(x, 'AppRun'),
                          'DownloadManager')
                time.sleep(1)
                break

        man=Manager()
    return man

def run_test(obj):
    """Test function."""
    p=0
    size=20*1024*1024
    while p<100:
        time.sleep(defdelay-2)
        p+=inc
        print p
        try:
            obj.update(p*size/100, size)
        except:
            print 'Cannot talk to server, ignoring'
    try:
        obj.done()
    except:
        print 'Could not tell server done'

if __name__=='__main__':
    obj=connect()
    obj.acquire('some-host.com', 'wibble.txt')
    try:
        run_test(obj)
    except:
        obj.cancel('Exception in client')
