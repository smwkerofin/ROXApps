import os, sys
import time
import gobject

import dbus, dbus.service, dbus.glib
try:
    import dbus.mainloop.glib
    if hasattr(dbus.mainloop.glib, 'DBusGMainLoop'):
        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
except ImportError:
    pass

# DBus setup
service_name='uk.co.demon.kerofin.NetStat.Usage'
interface_name=service_name
object_path='/Monitor'

interval=5*60
high_val=1<<32
max_days=60

xdg_config_home=os.getenv('XDG_CONFIG_HOME', os.path.expanduser('~/.config'))
xdg_config_dirs=os.getenv('XDG_CONFIG_DIRS', '/etc/config')
xdg_config_dirs=xdg_config_home+':'+xdg_config_dirs
if_dir=None
for d in xdg_config_dirs.split(':'):
    config_dir=os.path.join(d, 'kerofin.demon.co.uk', 'NetStat')
    if os.path.isdir(config_dir) and os.access(config_dir, os.W_OK):
        if_dir=os.path.join(config_dir, 'interfaces')
        if not os.path.isdir(if_dir):
            try:
                os.mkdir(if_dir)
                break
            except OSError:
                pass

if not if_dir:
    if_dir=os.path.join(xdg_config_home,'kerofin.demon.co.uk', 'NetStat',
                        'interfaces')
    os.makedirs(if_dir)

def get_day(timestamp):
    return time.localtime(timestamp).tm_yday

def get_start_time():
    """Return the time the system was booted."""

    if os.access('/proc/uptime', os.R_OK):
        try:
            up, idle=map(float, file('/proc/uptime', 'r').readline().split())
            return time.time()-up
        except:
            pass

    files=('/var/log/boot.msg', '/proc', '/sys')
    for f in files:
        try:
            s=os.stat(f)
            return s.st_mtime
        except OSError:
            pass

    return 0

class DailyUsage(object):
    def __init__(self, timestamp, rx, tx):
        self.timestamp=timestamp
        self.rx=rx
        self.tx=tx

    def __str__(self):
        date=time.strftime('%Y-%m-%d', time.localtime(self.timestamp))
        return '%s: %s/day Rx %s/day Tx' % (date, fmt_size(self.rx),
                                            fmt_size(self.tx))

    def for_csv(self):
        date=time.strftime('%Y-%m-%d', time.localtime(self.timestamp))
        return (date, self.rx, self.tx)

    def get_day(self):
        return get_day(self.timestamp)

class NetInterface(object):
    def __init__(self, name, rx, tx, timestamp):
        self.name=name
        self.last_update=timestamp

        self.filename=os.path.join(if_dir, self.name)

        # History
        self.days=[]

        # Today
        self.tx=tx
        self.tx_start=tx
        self.rx=rx
        self.rx_start=rx

        if os.access(self.filename, os.R_OK):
            self.read_file()
            self.update_file()

    def read_file(self):
        f=file(self.filename, 'r')
        ifname=f.readline().strip()
        assert ifname==self.name

        tstamp=int(f.readline().strip())
        now=time.time()
        boottime=get_start_time()

        rx, rx_start=map(long, f.readline().strip().split())
        tx, tx_start=map(long, f.readline().strip().split())

        if get_day(tstamp)!=get_day(now):
            yesterday=DailyUsage(tstamp, rx-rx_start, tx-tx_start)
            if boottime>tstamp:
                self.rx_start=0
                self.tx_start=0
            else:
                self.rx_start=rx
                self.tx_start=tx

        else:
            if boottime>tstamp:
                self.rx_start=0
                self.tx_start=0
            else:
                self.rx_start=rx_start
                self.tx_start=tx_start
            while self.rx<self.rx_start:
                self.rx+=high_val
            while self.tx<self.tx_start:
                self.tx+=high_val

            yesterday=None

        for line in f.readlines():
            l=line.strip()
            tstamp, rx, tx=map(int, l.split())

            day=DailyUsage(tstamp, rx, tx)
            self.days.append(day)

        if yesterday:
            if (len(self.days)>0 and
                get_day(yesterday.timestamp) ==
                             get_day(self.days[-1].timestamp)):
                self.days[-1]=yesterday
                
            else:
                self.days.append(yesterday)

        f.close()

    def update_file(self):
        f=file(self.filename, 'w')
        f.write('%s\n' % self.name)
        f.write('%d\n' % self.last_update)
        f.write('%d %d\n' % (self.rx, self.rx_start))
        f.write('%d %d\n' % (self.tx, self.tx_start))

        for day in self.days:
            f.write('%d %d %d\n' % (day.timestamp, day.rx, day.tx))

        f.close()

    def update_values(self, rx, tx, timestamp):
        while rx<self.rx:
            rx+=high_val
        while tx<self.tx:
            tx+=high_val

        if get_day(timestamp)!=get_day(self.last_update):
            yesterday=DailyUsage(self.last_update,
                                 self.rx-self.rx_start,
                                 self.tx-self.tx_start)
            self.days.append(yesterday)
            self.rx_start=rx
            self.tx_start=tx

        self.rx=rx
        self.tx=tx
        self.last_update=timestamp

        self.update_file()

    def get_total_usage(self, days=None):
        if days is None:
            days=len(self.days)
        req=self.days[-(days-1):]
        rx=self.rx-self.rx_start
        tx=self.tx-self.tx_start
        for day in req:
            rx+=day.rx
            tx+=day.tx

        return (len(req)+1, rx, tx)

    def get_duration(self):
        return len(self.days)+1

    def get_history(self):
        hist=[]
        for day in self.days:
            hist.append((day.timestamp, day.rx, day.tx))

        hist.append((time.time(), self.rx-self.rx_start,
                     self.tx-self.tx_start))

        return hist

    def __str__(self):
        return '%s: rx=%d tx=%d' % (self.name,
                                    self.rx-self.rx_start,
                                    self.tx-self.tx_start)
    def __repr__(self):
        return '<NetInterface %s %d,%d %d,%d %d>' % (self.name,
                                                     self.rx,
                                                     self.rx_start,
                                                     self.tx,
                                                     self.tx_start,
                                                     len(self.days))

class UsageMonitor(dbus.service.Object):
    def __init__(self, bus):
        bus_name=dbus.service.BusName(service_name, bus)
        dbus.service.Object.__init__(self, bus_name=bus_name,
                                     object_path=object_path)

        self.interfaces={}

        self.lookup_interfaces()

        gobject.timeout_add(interval*1000, self.update_interfaces)

        #print self
        #print dir(self)
        #print self.Ping()

    def lookup_interfaces(self):
        now=time.time()
        f=file('/proc/net/dev', 'r')

        f.readline()
        f.readline()
        for line in f.readlines():
            l=line.strip()
            name, other=l.split(':')
            vals=map(lambda x: int(x), other.split())
            rx=vals[0]
            tx=vals[8]

            self.interfaces[name]=NetInterface(name, rx, tx, now)

    def update_interfaces(self):
        now=time.time()
        f=file('/proc/net/dev', 'r')

        f.readline()
        f.readline()
        for line in f.readlines():
            l=line.strip()
            name, other=l.split(':')

            if name not in self.interfaces:
                self.interfaces[name]=NetInterface(name)

            dev=self.interfaces[name]
            vals=map(lambda x: int(x), other.split())
            rx=vals[0]
            tx=vals[8]
            #print dev.name, rx, tx, now
            dev.update_values(rx, tx, now)

        return True

    @dbus.service.method(dbus_interface=interface_name, out_signature='as')
    def GetInterfaces(self):
        names=self.interfaces.keys()
        return names

    @dbus.service.method(dbus_interface=interface_name,
                         in_signature='s', out_signature='itt')
    def GetInterfaceUsage(self, name):
        assert name in self.interfaces

        return self.interfaces[name].get_total_usage()

    @dbus.service.method(dbus_interface=interface_name,
                         in_signature='si', out_signature='itt')
    def GetInterfaceUsageOver(self, name, days):
        assert name in self.interfaces

        return self.interfaces[name].get_total_usage(days)

    @dbus.service.method(dbus_interface=interface_name,
                         in_signature='s', out_signature='i')
    def GetInterfaceDuration(self, name):
        assert name in self.interfaces

        return self.interfaces[name].get_duration()

    @dbus.service.method(dbus_interface=interface_name,
                         in_signature='s', out_signature='a(dtt)')
    def GetInterfaceHistory(self, name):
        assert name in self.interfaces

        hist=self.interfaces[name].get_history()
        #v=[]
        #for rx, tx in hist:
        #    v.append(rx)
        #    v.append(tx)

        return hist

def get_monitor(bus=None):
    if not bus:
        bus=dbus.SessionBus()
    return UsageMonitor(bus)

def check_daemon(bus=None):
    if not bus:
        bus=dbus.SessionBus()
    return bus.name_has_owner(service_name)

def run_daemon(bus=None):
    print 'running daemon'
    if not bus:
        bus=dbus.SessionBus()
    obj=get_monitor(bus)
    #print obj
    #print obj.interfaces
    mainloop=gobject.MainLoop()
    mainloop.run()
    
    print 'left main loop'
 
def start(bus=None):
    if not bus:
        bus=dbus.SessionBus()
    if bus.name_has_owner(service_name):
        return True

    print 'need to start'
    print __file__

    pid=os.spawnlp(os.P_NOWAIT, 'python', 'python', __file__)
    #pid=os.fork()
    #if pid==0:
    #    run_daemon()
    #    sys.exit(0)
    time.sleep(3)

    return pid

def client(bus=None):
    if not bus:
        bus=dbus.SessionBus()

    start(bus)

    obj=bus.get_object(service_name, object_path)
    return dbus.Interface(obj, interface_name)

if __name__=='__main__':
    #print start()
    run_daemon()
    

    
