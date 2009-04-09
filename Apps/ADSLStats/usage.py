import os
import sys
import time

import findrox; findrox.version(2, 0, 2)
import rox, rox.choices, rox.options
import gobject

import stats

offset=(sys.maxint+1L)*2

def fmt_size(x):
    if x>(4<<30):
        return '%d GB' % (x>>30)
    elif x>(4<<20):
        return '%d MB' % (x>>20)
    elif x>(4<<10):
        return '%d KB' % (x>>10)
    return '%d B' % x

def fmt_time(t):
    if t>172800:
        return '%.1f days' % (t/86400.)
    elif t>7200:
        return '%.1f hours' % (t/3600.)
    elif t>120:
        return '%.1f minutes' % (t/60.)
    return '%d seconds' % t

class DailyUsage(object):
    def __init__(self, timestamp=0, rx=0, tx=0, elapsed=0):
        self.timestamp=timestamp
        self.rx=rx
        self.tx=tx
        self.elapsed=elapsed

    def __str__(self):
        date=time.strftime('%Y-%m-%d', time.localtime(self.timestamp))
        return '%s: %s/day Rx %s/day Tx (%s up)' % (date, fmt_size(self.rx),
                                                    fmt_size(self.tx),
                                                    fmt_time(self.elapsed))

    def for_csv(self):
        date=time.strftime('%Y-%m-%d', time.localtime(self.timestamp))
        return (date, self.rx, self.tx)

    def get_day(self):
        return time.localtime(self.timestamp).tm_yday

class UsageList(object):
    def __init__(self, maxd=45, reportd=30):
        self.max=maxd
        self.days=reportd
        self.rx0=0
        self.rx=0
        self.rxh=0
        self.rx_base=0
        self.tx0=0
        self.tx=0
        self.txh=0
        self.tx_base=0
        self.previous=[]
        self.tstamp=None
        self.uptime=0
        self.window=None

        loaded=self.load()

        if not loaded:
            for d in rox.basedir.load_data_paths('kerofin.demon.co.uk',
                                                 'ADSLStats', 'usage'):
                try:
                    self.read_data(file(d))
                    loaded=True
                    break
                except OSError:
                    pass
        
        if not loaded:
            self.first_run()
        #gobject.timeout_add(60*60*1000, self.update)

    def first_run(self):
        now=time.time()
        tm=time.localtime(now)

        self.tstamp=now
        self.day=tm.tm_yday

        try:
            self.rx0, self.tx0, self.uptime=self.get_stats()
            self.rx=self.rx0
            self.tx=self.tx0
            self.previous=[]
        except:
            pass

    def get_stats(self):
        adsl=stats.open_adsl()
        atm=stats.open_atm()
        time.sleep(0.1)
        adsl.update()
        atm.update()
        v=atm.getVars()
        rx=int(v['Rx Bytes'])
        tx=int(v['Tx Bytes'])
        elapsed=adsl.get_elapsed()

        return rx, tx, elapsed

    def update(self):
        now=time.time()
        tm=time.localtime(now)
        rx, tx, elapsed=self.get_stats()

        if elapsed<self.uptime:
            # Modem has reset
            self.rxh=0
            self.txh=0
            self.rx_base=self.rx
            self.tx_base=self.tx
            self.rx0=0
            self.tx0=0
        self.uptime=elapsed
        
        rx+=self.rxh*offset
        while rx<self.rx0:
            self.rxh+=1
            rx+=offset
            
        tx+=self.txh*offset
        while tx<self.tx0:
            self.txh+=1
            tx+=offset
            
        
            
        if tm.tm_yday!=self.day:
            d=DailyUsage(self.tstamp,
                         rx-self.rx0+self.rx_base,
                         tx-self.tx0+self.tx_base,
                         elapsed)
            self.rx0=rx
            self.tx0=tx
            self.rx_base=0
            self.tx_base=0
            self.day=tm.tm_yday
            self.previous.append(d)
            if len(self.previous)>self.max:
                self.previous=self.previous[1:]
            self.save()

        self.rx=rx
        self.tx=tx

        self.tstamp=now

        if self.window:
            self.window.update()
        
        return True

    def report_day(self):
        print self.day, self.previous[-1]

    def get_report(self, days=30):
        days, rx, tx=self.get_summary(days)

        return 'Total for %d days: %s Rx %s Tx' % (days, fmt_size(rx),
                                                   fmt_size(tx))

    def get_summary(self, days=30):
        if len(self.previous)<days:
            days=len(self.previous)
        rx=self.rx-self.rx0+self.rx_base
        tx=self.tx-self.tx0+self.tx_base
        day=24*60*60
        now=time.time()
        early=now-day*days
        p=-1
        while True:
            #print fmt_size(rx)
            try:
                d=self.previous[p]
            except IndexError:
                break
            if d.timestamp<early:
                break
            #print p, d
            rx+=d.rx
            tx+=d.tx
            p-=1
        #print p, rx, tx

        return days, rx, tx

    def get_today(self):
        return self.rx-self.rx0+self.rx_base, self.tx-self.tx0+self.tx_base

    def as_csv(self):
        s='"Date","Received","Transmitted"\n'
        for r in self.previous:
            date, rx, tx=r.for_csv()
            s+='"%s",%d,%d\n' % (date, rx, tx)
        date=time.strftime('%Y-%m-%d', time.localtime(time.time()))
        s+='"%s",%d,%d\n' % (date, self.rx-self.rx0+self.rx_base,
                             self.tx-self.tx0+self.tx_base)
        return s

    def write(self, fout):
        fout.write('%ld %ld %ld\n' % (self.rx0, self.rx+self.rx_base,
                                      self.rxh))
        fout.write('%ld %ld %ld\n' % (self.tx0, self.tx+self.tx_base,
                                      self.txh))
        fout.write('%f\n' % self.tstamp)
        fout.write('%d\n' % self.uptime)
        for u in self.previous:
            fout.write('%f %ld %ld %d\n' % (u.timestamp, u.rx, u.tx,
                                            u.elapsed))

    def on_exit(self):
        self.save()
        
    def save(self):
        d=rox.basedir.save_data_path('kerofin.demon.co.uk', 'ADSLStats')
        fname=os.path.join(d, 'usage')
        #print fname
        self.write(file(fname, 'w'))

    def load(self):
        loaded=False
        for d in rox.basedir.load_data_paths('kerofin.demon.co.uk',
                                             'ADSLStats', 'usage'):
            #print d
            try:
                self.read_data(file(d))
                loaded=True
                break
            except OSError:
                pass
            except AssertionError:
                pass

        if loaded:
            self.update()
        return loaded

    def read_data(self, fin):
        adsl=stats.open_adsl()
        time.sleep(0.1)
        adsl.update()
        elapsed_time=adsl.get_elapsed()

        l=fin.readline().strip()
        rx0, rx, rxh=map(long, l.split())
        l=fin.readline().strip()
        tx0, tx, txh=map(long, l.split())

        lost=None
        tstamp=float(fin.readline().strip())
        sv_elapsed=int(fin.readline().strip())
        #print start_time, tstamp
        if elapsed_time<sv_elapsed:
            # Mode, has restarted since the file was saved, running
            # total cannot be valid
            #print start_time, tstamp
            tm_loaded=time.localtime(tstamp)
            tm_start=time.localtime(start_time)
            if tm_loaded.tm_yday!=tm_start.tm_yday:
                # new day, store what we had when we exited
                lost=DailyUsage(tstamp, rx-rx0, tx-tx0, sv_elapsed)
            else:
                # Set offset for today
                self.rx_base=rx-rx0
                self.tx_base=tx-tx0

            rx, tx, elapsed_time=self.get_stats()
            rx0=rx
            rxh=0
            tx0=tx
            tstamp=time.time()

        rec=[]
        rxt=0
        txt=0
        for l in fin.readlines():
            tok=l.strip().split()
            u=DailyUsage(float(tok[0]), long(tok[1]), long(tok[2]),
                         int(tok[3]))
            rec.append(u)
            rxt+=u.rx
            txt+=u.tx
        fin.close()

        self.rx0=rx0
        self.rx=rx
        self.rxh=rxh
        self.tx0=tx0
        self.tx=tx
        self.txh=txh
        self.tstamp=tstamp
        self.previous=rec
        self.uptime=elapsed_time

        now=time.time()
        tm=time.localtime(now)
        tm_loaded=time.localtime(tstamp)
        #print tm.tm_yday, tm_loaded.tm_yday
        if tm.tm_yday!=tm_loaded.tm_yday-1:
            if lost:
                d=lost
            else:
                #print self.rx, self.rx0
                d=DailyUsage(self.tstamp, self.rx-self.rx0, self.tx-self.tx0,
                             elapsed_time)
            self.rx0=self.rx
            self.tx0=self.tx
            self.day=tm.tm_yday
            #print d
            #print d.get_day(), self.previous[-1].get_day()
            if len(self.previous) and d.get_day()!=self.previous[-1].get_day():
                self.previous.append(d)

            if len(self.previous)>self.max:
                del self.previous[0]
            #print self.rx_tot, self.rx
            self.save()

        self.tstamp=now
        self.day=tm.tm_yday

    def get_data(self):
        return iter(self.previous)

    def set_period(self, ndays):
        self.days=ndays
        self.update()

    def open_window(self):
        if not self.window:
            self.window=UsageWindow(self)
        self.window.show()
        self.window.update()

class UsageWindow(rox.Window):
    def __init__(self, usage):
        rox.Window.__init__(self)
        self.usage=usage

        self.set_title('Bandwidth usage')

        table=rox.g.Table(6, 4)
        self.add(table)
        table.set_row_spacings(2)
        table.set_col_spacings(2)

        table.attach(rox.g.Label('Uptime'), 0, 1, 0, 1)
        self.uptime=rox.g.Label('')
        table.attach(self.uptime, 1, 2, 0, 1)

        table.attach(rox.g.Label('Total over'), 0, 1, 1, 2)
        self.days=rox.g.Label('')
        table.attach(self.days, 1, 2, 1, 2)

        table.attach(rox.g.Label('Receive'), 1, 2, 2, 3)
        table.attach(rox.g.Label('Transmit'), 2, 3, 2, 3)
        table.attach(rox.g.Label('Total'), 3, 4, 2, 3)

        table.attach(rox.g.Label('Today'), 0, 1, 3, 4)
        self.rx_today=rox.g.Label('')
        table.attach(self.rx_today, 1, 2, 3, 4)
        self.tx_today=rox.g.Label('')
        table.attach(self.tx_today, 2, 3, 3, 4)
        self.tot_today=rox.g.Label('')
        table.attach(self.tot_today, 3, 4, 3, 4)

        table.attach(rox.g.Label('Total'), 0, 1, 4, 5)
        self.rx_total=rox.g.Label('')
        table.attach(self.rx_total, 1, 2, 4, 5)
        self.tx_total=rox.g.Label('')
        table.attach(self.tx_total, 2, 3, 4, 5)
        self.tot_total=rox.g.Label('')
        table.attach(self.tot_total, 3, 4, 4, 5)

        table.attach(rox.g.Label('Average'), 0, 1, 5, 6)
        self.rx_average=rox.g.Label('')
        table.attach(self.rx_average, 1, 2, 5, 6)
        self.tx_average=rox.g.Label('')
        table.attach(self.tx_average, 2, 3, 5, 6)
        self.tot_average=rox.g.Label('')
        table.attach(self.tot_average, 3, 4, 5, 6)

        table.show_all()

        self.connect('delete_event', self.delete_event)

    def update(self):
        days, rx, tx=self.usage.get_summary()

        self.days.set_text('%2d days' % days)
        self.uptime.set_text(fmt_time(self.usage.uptime))

        self.rx_total.set_text(fmt_size(rx))
        self.tx_total.set_text(fmt_size(tx))
        self.tot_total.set_text(fmt_size(rx+tx))

        d=float(days+1)
        self.rx_average.set_text(fmt_size(int(rx/d)))
        self.tx_average.set_text(fmt_size(int(tx/d)))
        self.tot_average.set_text(fmt_size(int((rx+tx)/d)))

        rx, tx=self.usage.get_today()
        self.rx_today.set_text(fmt_size(rx))
        self.tx_today.set_text(fmt_size(tx))
        self.tot_today.set_text(fmt_size(rx+tx))

    def delete_event(self, *unused):
        self.hide()
        return True

def main():
    rep=len(sys.argv)>1
    if rep:
        delay=float(sys.argv[1])
    use=UsageList()
    use.update()
    while True:
        if rep:
            print time.ctime(time.time())
        use.update()
        print use.get_report()
        if rep:
            time.sleep(delay)
        else:
            break
        

if __name__=='__main__':
    main()
    
