#!/bin/sh
#
# Locate a named AppDir in LIBDIRPATH, optionally checking version
#
# $Id: libdir.sh,v 1.3 2004/07/07 20:23:33 stephen Exp $

if [ x"$LIBDIRPATH" = x ]; then
  LIBDIRPATH="$HOME/lib:/usr/local/lib:/usr/lib"
  export LIBDIRPATH
fi

if [ x"$APPDIRPATH" = x ]; then
  APPDIRPATH="$HOME/Apps:/usr/local/apps:/usr/apps"
  export APPDIRPATH
fi

mode="@mode@"

if [ $mode = libdir ]; then
  path="$LIBDIRPATH"
  zero_dir=lib
else
  path="$APPDIRPATH"
  zero_dir=apps
fi

usage() {
  echo "$prog [--version x.y.z] [--libdir|--appdir|--path PATH]" >&2
  echo " [--0install site] [--append-path dir] [--prepend-path dir] App" >&2
  echo "where" >&2
  echo "  --version x.y.z    only look for verison x.y.z or later" >&2
  echo "  --libdir           use LIBDIRPATH $LIBDIRPATH" >&2
  echo "  --appdir           use APPDIRPATH $APPDIRPATH" >&2
  echo "  --path PATH        use given PATH" >&2
  echo "  --0install site    look on the Zero Install site, if available" >&2
  echo "  --append-path dir  append dir onto the current path" >&2
  echo "  --prepend-path dir prepend dir onto the current path" >&2
  echo "   (default is $path)" >&2
  echo "  App                Application or library to find"  >&2

  exit 3
}

if [ x"$1" = x ]; then
  usage
fi

rver=
app=""

while [ "$1"x != "x" ]; do
    case "$1" in 
    --version) 
    rver=$2
    shift 2;;
    --appdir)
    path="$APPDIRPATH"
    zero_dir=apps
    shift;;
    --libdir)
    path="$LIBDIRPATH"
    zero_dir=lib
    shift;;
    --path)
    path="$2"
    shift 2;;
    --append-path)
    path="$path:$2"
    shift 2;;
    --prepend-path)
    path="$2:$path"
    shift 2;;
    --0install)
    path="$path:/uri/0install/$2/${zero_dir}"
    shift 2;;
    *)
    app="$1"
    shift;;
  esac
done

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
