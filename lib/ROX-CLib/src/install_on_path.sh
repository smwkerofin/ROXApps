#!/bin/sh
#
# $Id$
#
# install_on_path file_to_install

if [ $# -lt 1 ] ; then
    echo Install which file\?
    exit 2
fi

file=$1
base=`basename $file`

if [ ! -r "$file" ] ; then
    echo $file not found
    exit 3
fi

for dir in `echo $PATH | sed -e 's/:/ /g'` ; do
    # echo $dir
    if [ -x $dir/$base ]; then
	echo $base already installed in $dir
	exit 0
    fi
done

for dir in `echo $PATH | sed -e 's/:/ /g'` ; do
    # echo $dir
    if [ -x $dir/$base ]; then
	echo $base exists in $dir
	exit 0
    fi
    if [ -w $dir ]; then
	echo "Install $1 in $dir (y/N) \c"
	read ans
	if [ "$ans" = "y" -o "$ans" = "Y" ]; then
	    exec cp -f $file $dir
	fi
    fi
done
