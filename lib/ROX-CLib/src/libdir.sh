#!/bin/sh
#
# Locate a named AppDir in LIBDIRPATH, optionally checking version
#
# $Id: libdir.sh,v 1.1 2004/05/31 10:47:35 stephen Exp $

prog=`basename $0`

if [ x"$LIBDIRPATH" = x ]; then
  LIBDIRPATH="$HOME/lib:/usr/local/lib:/usr/lib"
  export LIBDIRPATH
fi

if [ x"$APPDIRPATH" = x ]; then
  APPDIRPATH="$HOME/Apps:/usr/local/apps:/usr/apps"
  export APPDIRPATH
fi

path="@DEFAULT_PATH@"

usage() {
  echo "$prog [--version x.y.z] [--libdir|--appdir|--path PATH] App" >&2
  echo "  --version x.y.z    only look for verison x.y.z or later" >&2
  echo "  --libdir           use LIBDIRPATH $LIBDIRPATH" >&2
  echo "  --appdir           use APPDIRPATH $APPDIRPATH" >&2
  echo "  --path PATH        use given PATH" >&2
  echo "   (default is $path)"

  exit 3
}

if [ x"$1" = x ]; then
  usage
fi

rver=

case "$1" in 
  --version) 
   rver=$2
   shift 2;;
  --appdir)
   path="$APPDIRPATH"
   shift;;
  --libdir)
   path="$LIBDIRPATH"
   shift;;
  --path)
   path="$2"
   shift 2;;
  --append-path)
   path="$PATH:$2"
   shift 2;;
  --prepend-path)
   path="$2:$PATH"
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
for dir in $path; do
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
