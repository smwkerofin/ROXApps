dnl autoconf checks for ROX-CLib (and ROX in general)

AC_DEFUN(ROX_FIND_ROX_RUN, [
if test -z "$ROX_RUN"; then
  AC_CHECK_PROG(ROX_RUN, rox_run, rox_run, "$APP_DIR/rox_run")
fi
AC_SUBST(ROX_RUN)
])

dnl Check for app
dnl ROX_APP_DIR(app, [var])
AC_DEFUN(ROX_APP_DIR, [
AC_REQUIRE([ROX_FIND_ROX_RUN])
AC_MSG_CHECKING($1)
if "$ROX_RUN" $1 -h > /dev/null 2>&1 ; then
  if test $# -gt 1; then
    AC_DEFINE_UNQUOTED($2, 1)
  fi
  AC_MSG_RESULT(yes)
else
  if test $# -gt 1; then
    AC_MSG_RESULT(no)
    AC_DEFINE_UNQUOTED($2, 0)
  else
    AC_MSG_ERROR(can't run $1)
  fi
fi
])

dnl Check for app and version
dnl ROX_APP_DIR_VERSION(app, arg, major, minor, rel, ver_var)
AC_DEFUN(ROX_APP_DIR_VERSION, [
ROX_APP_DIR($1)
AC_MSG_CHECKING(version of $1 >= $3.$4.$5)

ac_verstr=`"$ROX_RUN" $1 $2`
ac_ver=`echo $ac_verstr | cut -d " " -f 2`
set `echo $ac_ver | tr '.' ' '`
ac_version=`expr $[1] \* 10000 + $[2] \* 100 + $[3]`
ac_rversion=`expr $3 \* 10000 + $4 \* 100 + $5`
if test $# -gt 5; then
  AC_DEFINE_UNQUOTED($6, $ac_version)
fi
if test $ac_version -lt $ac_rversion ; then
  AC_MSG_ERROR($1 is too old.  Need $3.$4.$5 or later not $ac_verstr)
else
  AC_MSG_RESULT(version $ac_ver)
fi
])

dnl ROX-CLib specific
AC_DEFUN(ROX_CLIB, [
ROX_APP_DIR_VERSION(ROX-CLib, -v, $1, $2, $3, ROX_CLIB_VERSION)
])

dnl Extract version number from $APP_DIR/AppInfo.xml
AC_DEFUN(ROX_SELF_VERSION, [
AC_MSG_CHECKING(version information)
if test "$APP_DIR" = "" ; then
  ac_app_info=../AppInfo.xml
else
  ac_app_info="$APP_DIR/AppInfo.xml"
fi
if test -r "$ac_app_info" ; then
changequote(,)dnl
VERSION=`sed -n 's/^.*<Version>\([^<]*\)<\/Version>.*$/\1/p' "$ac_app_info"`
changequote([,])dnl
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_MSG_RESULT(version $VERSION)
else
AC_MSG_ERROR(AppInfo.xml not found)
fi
])
