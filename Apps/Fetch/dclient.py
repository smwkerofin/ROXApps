import os, sys, time
import dbus

service_name='uk.co.demon.kerofin.DownloadManager'
interface_name=service_name
object_path='/DownloadManager'

defdelay=5
inc=5

class DMException(Exception):
    pass

class DMNotRunning(DMException):
    pass

class DMNoAnswer(DMException):
    pass

class Manager:
    def __init__(self, service=service_name, object=object_path):
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
        try:
            return self.object.QueueSize()
        except:
            pass
        raise DMNoAnswer

    def acquire(self, host, fname, block=True, delay=defdelay):
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
        try:
            self.object.Update(size, total)
        except:
            pass

    def done(self):
        try:
            self.object.Done()
        except:
            pass

    def cancel(self, why):
        try:
            self.object.Cancel(why)
        except:
            pass

def connect(start=False):
    try:
        man=Manager()
    except DMNotRunning:
        if not start:
            raise

        # Start DownloadManager here

        man=Manager()
    return man

def run_test(obj):
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
