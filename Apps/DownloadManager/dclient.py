import os, sys, time
import dbus

# DBus setup
service_name='uk.co.demon.kerofin.DownloadManager'
interface_name=service_name
object_path='/DownloadManager'

defdelay=5
inc=5

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
        self.bus=dbus.Bus(dbus.Bus.TYPE_SESSION)
        self.service=self.bus.get_service(service)
        interface_name=service
        self.object=self.service.get_object(object, interface_name)

        running=False
        try:
            running=self.object.Ping()
        except:
            pass
        if not running:
            raise DMNotRunning

    def getQueueSize(self):
        """Return number of clients in the queue waiting to start.  If
        we are waiting then this will include us."""
        try:
            return self.object.QueueSize()
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
                ok=self.object.CanIStart(host, fname)
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
            self.object.Update(size, total)
        except:
            pass

    def done(self):
        """Download has finished sucessfully"""
        try:
            self.object.Done()
        except:
            pass

    def cancel(self, why):
        """Download was cancelled, either by user intervention or some error.
        why - string describing the reason the download was cancelled"""
        try:
            self.object.Cancel(why)
        except:
            pass

def connect(start=False):
    """Make connection to DownloadManager.
    start - if True and the DownloadManager is not running attempt to start it
            (defaults to False) (not yet implemented)
    Returns a Manager object representing the connect.
    Can raise a DMException."""
    try:
        man=Manager()
    except DMNotRunning:
        if not start:
            raise

        # Start DownloadManager here

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
