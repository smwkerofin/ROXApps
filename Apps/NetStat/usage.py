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
import usage_daemon

offset=(sys.maxint+1L)*2
daemon=usage_daemon.client()

def fmt_size(x):
    if x>(4<<30):
        return '%d GB' % (x>>30)
    elif x>(4<<20):
        return '%d MB' % (x>>20)
    elif x>(4<<10):
        return '%d KB' % (x>>10)
    return '%d B' % x

def get_day(timestamp):
    return time.localtime(timestamp).tm_yday

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

class UsageList(object):
    def __init__(self, ifname, maxd=45, reportd=30):
        self.name=ifname
        self.max=maxd
        self.days=reportd

        self.rx_tot=0
        self.tx_tot=0

        self.previous=[]
        self.tstamp=None
        self.daemon=daemon

        self.update()


    def update(self):
        self.previous=[]
        self.rx_tot=0
        self.tx_tot=0
        now=time.time()
        today=get_day(now)
        for dat in self.daemon.GetInterfaceHistory(self.name):
            tstamp=float(dat[0])
            rx=long(dat[1])
            tx=long(dat[2])

            day=get_day(tstamp)
            if today-day<=self.days:
                self.rx_tot+=rx
                self.tx_tot+=tx
                self.previous.append(DailyUsage(tstamp, rx, tx))

        self.tstamp=now
        self.day=today
                
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
        rx=0
        tx=0
        for i in range(days):
            p=-1-i
            d=self.previous[p]
            #print d
            rx+=d.rx
            tx+=d.tx

        return days, rx, tx

    def get_today(self):
        return self.previous[-1].rx, self.previous[-1].tx

    def as_csv(self):
        s='"Date","Received","Transmitted"\n'
        for r in self.previous:
            date, rx, tx=r.for_csv()
            s+='"%s",%d,%d\n' % (date, rx, tx)
        return s

    def get_data(self):
        return iter(self.previous)

    def set_interface(self, ifname):
        if ifname!=self.name:
            oname=self.name
            try:
                self.name=ifname
                self.update()
            except KeyError:
                self.name=oname

    def set_period(self, ndays):
        self.days=ndays
        self.update()

C_DATE=0
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
        self.last_list_update=0

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
        self.last_list_update=0
        today=get_day(time.time())
        for u in self.usage_data.get_data():
            day=u.get_day()
            if day!=today and today-day<=self.days:
                it=store.append()
                ts, rx, tx=u.for_csv()
                store.set(it, C_DATE, ts, C_RX, fmt_size(rx),
                          C_TX, fmt_size(tx), C_TOTAL, fmt_size(rx+tx),
                          C_DATA, u)

                self.last_list_update=ts
        

    def update_list(self):
        #print 'update_list'
        store=self.usage_list.get_model()
        for u in self.usage_data.get_data():
            ts, rx, tx=u.for_csv()
            print ts, rx, tx
            if ts>self.last_list_update:
                it=store.append()
                store.set(it, C_DATE, ts, C_RX, fmt_size(rx),
                          C_TX, fmt_size(tx), C_TOTAL, fmt_size(rx+tx),
                          C_DATA, u)

                self.last_list_update=ts

        today=get_day(time.time())
        cont=True
        while cont:
            it=store.get_iter_first()
            if it:
                u=store[it][C_DATA]
                day=u.get_day()
                #print today, day, u
                if today-day>=self.days:
                    store.remove(it)
                else:
                    cont=False
                

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

    def set_interface(self, ifname):
        if ifname!=self.ifname:
            self.usage_data.set_interface(ifname)
            self.ifname=self.usage_data.name
            self.usage_if.set_text(self.ifname)
            self.load_list()
            self.do_update()

    def set_period(self, ndays):
        trim=ndays<self.days
        self.days=ndays
        self.usage_data.set_period(self.days)
        self.do_update()
        if trim:
            self.update_list()

    def set_limits(self, rx, tx, total):
        self.rx_limit=rx
        self.tx_limit=tx
        self.total_limit=total
        self.do_update()

    def hide_window(self, widget, udata):
        self.hide()
        return True

    def on_exit(self):
        # No longer needed
        pass

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
    #win.set_period(3)
    try:
        win.show()
        rox.mainloop()
    except:
        pass

if __name__=='__main__':
    test_gui()
