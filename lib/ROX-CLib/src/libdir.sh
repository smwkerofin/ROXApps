#!/bin/sh
#
# Locate a named AppDir in LIBDIRPATH, optionally checking version
#
# $Id$

usage() {
  echo "libdir [--version x.y.z] App" >&2
  exit 3
}

if [ x"$1" = x ]; then
  usage
fi

rver=

if [ x"$LIBDIRPATH" = x ]; then
  LIBDIRPATH="$HOME/lib:/usr/local/lib:/usr/lib"
  export LIBDIRPATH
fi

case "$1" in 
  --version) 
   rver=$2
   shift 2;;
esac
if [ x"$1" = x ]; then
  usage
fi

app="$1"
found=0

test_version() {
  app="$1"
  rverstr="$2"

  if [ -r "$app/AppInfo.xml" ]; then
    verstr=`sed -n 's/^.*<Version>\([^< ]*\) .*<\/Version>.*$/\1/p' "$app/AppInfo.xml"`
    IFS=.
    set $verstr
    ver=`expr $1 \* 10000 + $2 \* 100 + $3`
    set $rverstr
    rver=`expr $1 \* 10000 + $2 \* 100 + $3`
    if [ $ver -lt $rver ]; then
      echo 0
    else
      echo 1
    fi
  else
    echo 0
  fi
}

IFS=:
for dir in $LIBDIRPATH; do
  if [ -x "$dir/$app/AppRun" ]; then
    found=1
    if [ x"$rver" = x ]; then
      echo "$dir/$app"
      exit 0
    fi
    ok=`test_version "$dir/$app" $rver`
    if [ $ok = 1 ]; then
      echo "$dir/$app"
      exit 0
    fi
  fi 
done

if [ $found = 1 ] ; then
  exit 2
fi

exit 1
