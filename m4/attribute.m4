dnl Check to find out whether function attributes are supported.
dnl If they are not, #define them to be nothing.

AC_DEFUN([MeshLink_ATTRIBUTE],
[
  AC_CACHE_CHECK([for working $1 attribute], MeshLink_cv_attribute_$1,
  [ 
    tempcflags="$CFLAGS"
    CFLAGS="$CFLAGS -Wall -Werror"
    AC_COMPILE_IFELSE(
      [AC_LANG_SOURCE(
        [void *test(void *arg) __attribute__ (($1));
	 void *test(void *arg) { return arg; }
	],
       )],
       [MeshLink_cv_attribute_$1=yes],
       [MeshLink_cv_attribute_$1=no]
     )
     CFLAGS="$tempcflags"
   ])

   if test ${MeshLink_cv_attribute_$1} = no; then
     AC_DEFINE([$1], [], [Defined if the $1 attribute is not supported.])
   fi
])
