import rox, rox.options

rox.setup_app_options('DownloadManager', site='kerofin.demon.co.uk')

allow_nclient=rox.options.Option('allow_nclient', 1)
client_expires=rox.options.Option('client_expires', 60)
show_nactive=rox.options.Option('show_nactive', 2)
show_nwaiting=rox.options.Option('show_nwaiting', 1)

rox.app_options.notify()

