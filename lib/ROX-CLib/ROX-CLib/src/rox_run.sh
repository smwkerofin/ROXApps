#!/bin/sh

prog=$1
if [ -z "$prog" ]; then
    echo "run_rox: no arguments given" >&2
    echo "usage: run_rox program [arguments]" >&2
    exit 127
fi

shift

echo $prog | grep / > /dev/null 2>&1
if [ $? -eq 0 ]; then
    exec rox $prog $@
fi

for dir in  `echo $APPPATH $PATH | sed 's/:/ /g'`; do
    if [ -d $dir/$prog -a -x $dir/$prog/AppRun ]; then
	$dir/$prog/AppRun $@ & 
	exit 0
    elif [ -x $dir/$prog ]; then
	exec $dir/$prog $@
    elif [ -r $dir/$prog ]; then
	exec rox $dir/$prog
    fi
done

echo "run_rox: $prog not found" >&2
exit 127
