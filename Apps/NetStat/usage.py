import os, sys
import time
import gobject

if __name__=='__main__':
    import findrox; findrox.version(2, 0, 0)
    import rox

    __builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))
else:
    import rox
    
import rox.basedir
import rox.templates

import gobject

import netstat

offset=(sys.maxint+1L)*2

def fmt_size(x):
    if x>(4<<30):
        return '%d GB' % (x>>30)
    elif x>(4<<20):
        return '%d MB' % (x>>20)
    elif x>(4<<10):
        return '%d KB' % (x>>10)
    return '%d B' % x

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
        return time.localtime(self.timestamp).tm_yday

class UsageList(object):
    def __init__(self, ifname, maxd=45, reportd=30):
        self.name=ifname
        self.max=maxd
        self.days=reportd
        self.rx0=0
        self.rx=0
        self.rxh=0
        self.rx_tot=0
        self.rx_base=0
        self.tx0=0
        self.tx=0
        self.txh=0
        self.tx_tot=0
        self.tx_base=0
        self.previous=[]
        self.tstamp=None

        loaded=self.load()

        if not loaded:
            for d in rox.basedir.load_data_paths('kerofin.demon.co.uk',
                                                 'NetStat', 'usage'):
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

        stats=netstat.stat()[self.name]
        #print stats
        self.rx0=stats[2]
        self.tx0=stats[3]
        self.rx=self.rx0
        self.tx=self.tx0
        self.previous=[]

    def update(self):
        now=time.time()
        tm=time.localtime(now)
        stats=netstat.stat()[self.name]
        rx, tx=stats[2:4]
        self.rx=rx+self.rxh*offset
        while self.rx<self.rx0:
            self.rxh+=1
            self.rx=rx+self.rxh*offset
        while self.tx<self.tx0:
            self.txh+=1
            self.tx=tx+self.txh*offset
        self.tx=tx+self.txh*offset

        if tm.tm_yday!=self.day:
            d=DailyUsage(self.tstamp,
                         self.rx-self.rx0+self.rx_base,
                         self.tx-self.tx0+self.tx_base)
            self.rx_tot+=d.rx
            self.tx_tot+=d.tx
            self.rx0=self.rx
            self.tx0=self.tx
            self.rx_base=0
            self.tx_base=0
            self.day=tm.tm_yday
            self.previous.append(d)
            if len(self.previous)>self.max:
                self.rx_tot-=self.previous[0].rx
                self.tx_tot-=self.previous[0].tx
                self.previous=self.previous[1:]
            self.save()

            #self.report_day()

        self.tstamp=now
        
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
        for i in range(days):
            p=-1-i
            d=self.previous[p]
            #print d
            rx+=d.rx
            tx+=d.tx

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
        fout.write(self.name+'\n')
        fout.write('%ld %ld %ld\n' % (self.rx0, self.rx+self.rx_base,
                                      self.rxh))
        fout.write('%ld %ld %ld\n' % (self.tx0, self.tx+self.tx_base,
                                      self.txh))
        fout.write('%f\n' % self.tstamp)
        for u in self.previous:
            fout.write('%f %ld %ld\n' % (u.timestamp, u.rx, u.tx))

    def on_exit(self):
        self.save()
        
    def save(self):
        d=rox.basedir.save_data_path('kerofin.demon.co.uk', 'NetStat')
        fname=os.path.join(d, 'usage:'+self.name)
        #print fname
        self.write(file(fname, 'w'))

    def load(self):
        loaded=False
        for d in rox.basedir.load_data_paths('kerofin.demon.co.uk',
                                             'NetStat', 'usage:'+self.name):
            #print d
            try:
                self.read_data(file(d))
                loaded=True
                break
            except OSError:
                pass
            except AssertionError:
                pass

        return loaded

    def read_data(self, fin):
        start_time=netstat.get_start_time()
        name=fin.readline().strip()
        assert name==self.name

        l=fin.readline().strip()
        rx0, rx, rxh=map(long, l.split())
        l=fin.readline().strip()
        tx0, tx, txh=map(long, l.split())

        lost=None
        tstamp=float(fin.readline().strip())
        #print start_time, tstamp
        if start_time>0 and tstamp<start_time:
            # System has restarted since the file was saved, running
            # total cannot be valid
            #print start_time, tstamp
            tm_loaded=time.localtime(tstamp)
            tm_start=time.localtime(start_time)
            if tm_loaded.tm_yday!=tm_start.tm_yday:
                # new day, store what we had when we exited
                lost=DailyUsage(tstamp, rx-rx0, tx-tx0)
            else:
                # Set offset for today
                self.rx_base=rx-rx0
                self.tx_base=tx-tx0
            stats=netstat.stat()[name]
            rx0=stats[2]
            rx=stats[2]
            rxh=0
            tx0=stats[3]
            tx=stats[3]
            txh=0
            tstamp=time.time()

        rec=[]
        rxt=0
        txt=0
        for l in fin.readlines():
            tok=l.strip().split()
            u=DailyUsage(float(tok[0]), long(tok[1]), long(tok[2]))
            rec.append(u)
            rxt+=u.rx
            txt+=u.tx
        fin.close()

        self.rx0=rx0
        self.rx=rx
        self.rxh=rxh
        self.rx_tot=rxt
        self.tx0=tx0
        self.tx=tx
        self.txh=txh
        self.tx_tot=txt
        self.tstamp=tstamp
        self.previous=rec

        now=time.time()
        tm=time.localtime(now)
        tm_loaded=time.localtime(tstamp)
        if tm.tm_yday!=tm_loaded.tm_yday:
            if lost:
                d=lost
            else:
                d=DailyUsage(self.tstamp, self.rx-self.rx0, self.tx-self.tx0)
            self.rx_tot+=d.rx
            self.tx_tot+=d.tx
            self.rx0=self.rx
            self.tx0=self.tx
            self.day=tm.tm_yday
            if d.get_day()!=self.previous[-1].get_day():
                self.previous.append(d)
            else:
                self.previous[-1]=d
            if len(self.previous)>self.max:
                self.rx_tot-=self.previous[0].rx
                self.tx_tot-=self.previous[0].tx
                del self.previous[0]
            self.save()            

        self.tstamp=now
        self.day=tm.tm_yday

    def get_data(self):
        return iter(self.previous)

    def set_interface(self, ifname):
        if ifname!=self.name:
            oname=self.name
            try:
                self.save()
                self.name=ifname
                if not self.load():
                    self.first_run()
                self.update()
            except KeyError:
                self.name=oname

    def set_period(self, ndays):
        self.days=ndays
        self.update()

C_RX=1
C_TX=2
C_TOTAL=3
C_DATA=4

class UsageWindow(rox.templates.ProxyWindow):
    def __init__(self, window, widgets, *args):
        rox.templates.ProxyWindow.__init__(self, window, widgets)

        self.ifname=args[0]
        self.days=30
        self.rx_limit=0
        self.tx_limit=0
        self.total_limit=0

        self.usage_if=widgets['usage_if']
        self.usage_list=widgets['usage_list']
        self.usage_days=widgets['usage_days']
        self.usage_rx_today=widgets['usage_rx_today']
        self.usage_tx_today=widgets['usage_tx_today']
        self.usage_total_today=widgets['usage_total_today']
        self.usage_rx_total=widgets['usage_rx_total']
        self.usage_tx_total=widgets['usage_tx_total']
        self.usage_total_total=widgets['usage_total_total']
        self.usage_rx_av=widgets['usage_rx_av']
        self.usage_tx_av=widgets['usage_tx_av']
        self.usage_total_av=widgets['usage_total_av']

        self.usage_data=UsageList(self.ifname)

        self.usage_if.set_text(self.ifname)

        store=rox.g.ListStore(str, str, str, str, object)
        self.usage_list.set_model(store)

        cell=rox.g.CellRendererText()
        column=rox.g.TreeViewColumn(_('Date'), cell, text=C_DATE)
        self.usage_list.append_column(column)

        cell=rox.g.CellRendererText()
        column=rox.g.TreeViewColumn(_('Rx'), cell, text=C_RX)
        self.usage_list.append_column(column)

        cell=rox.g.CellRendererText()
        column=rox.g.TreeViewColumn(_('Tx'), cell, text=C_TX)
        self.usage_list.append_column(column)

        cell=rox.g.CellRendererText()
        column=rox.g.TreeViewColumn(_('Total'), cell, text=C_TOTAL)
        self.usage_list.append_column(column)

        if hasattr(rox, 'StatusIcon'):
            self.sicon=rox.StatusIcon(add_ref=False, show=False,
                                      icon_stock=rox.g.STOCK_DIALOG_WARNING)
        else:
            self.sicon=None

        self.load_list()
        self.update_stats()

        #gobject.timeout_add(20*1000, self.do_update)

    def load_list(self):
        store=self.usage_list.get_model()
        store.clear()
        for u in self.usage_data.get_data():
            it=store.append()
            ts, rx, tx=u.for_csv()
            store.set(it, C_DATE, ts, C_RX, fmt_size(rx),
                      C_TX, fmt_size(tx), C_TOTAL, fmt_size(rx+tx),
                      C_DATA, u)

        self.last_list_update=ts

    def update_list(self):
        store=self.usage_list.get_model()
        for u in self.usage_data.get_data():
            ts, rx, tx=u.for_csv()
            if ts>self.last_list_update:
                it=store.append()
                store.set(it, C_DATE, ts, C_RX, fmt_size(rx),
                          C_TX, fmt_size(tx), C_TOTAL, fmt_size(rx+tx),
                          C_DATA, u)

        self.last_list_update=ts

    def update_stats(self):
        rxt, txt=self.usage_data.get_today()
        self.usage_rx_today.set_text(fmt_size(rxt))
        self.usage_tx_today.set_text(fmt_size(txt))
        self.usage_total_today.set_text(fmt_size(rxt+txt))
        
        days, rx, tx=self.usage_data.get_summary(self.days)
        
        self.usage_days.set_text(str(days))

        self.usage_rx_total.set_text(fmt_size(rx))
        self.usage_tx_total.set_text(fmt_size(tx))
        self.usage_total_total.set_text(fmt_size(rx+tx))
        if days>0:
            self.usage_rx_av.set_text(fmt_size((rx-rxt)/days))
            self.usage_tx_av.set_text(fmt_size((tx-txt)/days))
            self.usage_total_av.set_text(fmt_size((rx-rxt+tx-txt)/days))
        else:
            self.usage_rx_av.set_text('-')
            self.usage_tx_av.set_text('-')
            self.usage_total_av.set_text('-')

        #print self.rx_limit, rx>>30
        wrx=False
        wtx=False
        wtotal=False
        if self.rx_limit>0:
            wrx=(rx>>30)>=self.rx_limit*0.9
        if self.tx_limit>0:
            wtx=(tx>>30)>=self.tx_limit*0.9
        if self.total_limit>0:
            wtotal=((rx+tx)>>30)>=self.total_limit*0.9

        if self.sicon:
            self.sicon.set_visible(wrx or wtx or wtotal)

            s=''
            if wrx:
                s+='RX %s ' % fmt_size(rx)
            if wtx:
                s+='TX %s ' % fmt_size(tx)
            if wtotal:
                s+='Total %s ' % fmt_size(rx+tx)
            self.sicon.set_tooltip(s)

    def do_update(self):
        day=self.usage_data.day
        self.usage_data.update()
        self.update_stats()
        if day!=self.usage_data.day:
            self.update_list()
        
        return True

    def on_exit(self):
        self.usage_data.on_exit()

    def set_interface(self, ifname):
        if ifname!=self.ifname:
            self.usage_data.set_interface(ifname)
            self.ifname=self.usage_data.name
            self.usage_if.set_text(self.ifname)
            self.load_list()
            self.do_update()

    def set_period(self, ndays):
        self.days=ndays
        self.usage_data.set_period(self.days)
        self.do_update()

    def set_limits(self, rx, tx, total):
        self.rx_limit=rx
        self.tx_limit=tx
        self.total_limit=total
        self.do_update()

    def hide_window(self, widget, udata):
        self.hide()
        return True

C_DATE=0
widgets=rox.templates.load('usagewin')

def get_window(ifname):
    return widgets.get_window('usagewin', UsageWindow, ifname)

def test_no_gui():
    mloop=gobject.MainLoop()

    usage=UsageList('eth0')
    try:
        mloop.run()
    except KeyboardInterrupt:
        print usage.get_report()
    usage.write(sys.stdout)
    usage.on_exit()

def test_gui():
    win=get_window('eth0')
    try:
        win.show()
        rox.mainloop()
    except:
        pass
    win.on_exit()

if __name__=='__main__':
    test_gui()
