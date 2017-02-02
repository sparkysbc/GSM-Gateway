if test "x${PBX_ALLOGSMAT}" != "x1" -a "${USE_ALLOGSMAT}" != "no"; then
   pbxlibdir=""
   # if --with-allogsmat=DIR has been specified, use it.
   if test "x${ALLOGSMAT_DIR}" != "x"; then
      if test -d ${ALLOGSMAT_DIR}/lib; then
         pbxlibdir="-L${ALLOGSMAT_DIR}/lib"
      else
         pbxlibdir="-L${ALLOGSMAT_DIR}"
      fi
   fi
   pbxfuncname="allogsm_new"
   if test "x${pbxfuncname}" = "x" ; then   # empty lib, assume only headers
      AST_ALLOGSMAT_FOUND=yes
   else
      ast_ext_lib_check_save_CFLAGS="${CFLAGS}"
      CFLAGS="${CFLAGS} "
      as_ac_Lib=`echo "ac_cv_lib_allogsmat_${pbxfuncname}" | $as_tr_sh`
{ echo "$as_me:$LINENO: checking for ${pbxfuncname} in -lallogsmat" >&5
echo $ECHO_N "checking for ${pbxfuncname} in -lallogsmat... " >&6; }
if { as_var=$as_ac_Lib; eval "test \"\${$as_var+set}\" = set"; }; then
  echo $ECHO_N "(cached) " >&6
else
  ac_check_lib_save_LIBS=$LIBS
LIBS="-lallogsmat ${pbxlibdir}  $LIBS"
cat >conftest.$ac_ext <<_ACEOF
/* confdefs.h.  */
_ACEOF
cat confdefs.h >>conftest.$ac_ext
cat >>conftest.$ac_ext <<_ACEOF
/* end confdefs.h.  */

/* Override any GCC internal prototype to avoid an error.
   Use char because int might match the return type of a GCC
   builtin and then its argument prototype would still apply.  */
#ifdef __cplusplus
extern "C"
#endif
char ${pbxfuncname} ();
int
main ()
{
return ${pbxfuncname} ();
  ;
  return 0;
}
_ACEOF
rm -f conftest.$ac_objext conftest$ac_exeext
if { (ac_try="$ac_link"
case "(($ac_try" in
  *\"* | *\`* | *\\*) ac_try_echo=\$ac_try;;
  *) ac_try_echo=$ac_try;;
esac
eval ac_try_echo="\"\$as_me:$LINENO: $ac_try_echo\""
echo "$ac_try_echo") >&5
  (eval "$ac_link") 2>conftest.er1
  ac_status=$?
  grep -v '^ *+' conftest.er1 >conftest.err
  rm -f conftest.er1
  cat conftest.err >&5
  echo "$as_me:$LINENO: \$? = $ac_status" >&5
  (exit $ac_status); } && {
         test -z "$ac_c_werror_flag" ||
         test ! -s conftest.err
       } && test -s conftest$ac_exeext && {
         test "$cross_compiling" = yes ||
         $as_test_x conftest$ac_exeext
       }; then
  eval "$as_ac_Lib=yes"
else
  echo "$as_me: failed program was:" >&5
sed 's/^/| /' conftest.$ac_ext >&5

        eval "$as_ac_Lib=no"
fi

rm -rf conftest.dSYM
rm -f core conftest.err conftest.$ac_objext conftest_ipa8_conftest.oo \
      conftest$ac_exeext conftest.$ac_ext
LIBS=$ac_check_lib_save_LIBS
fi
ac_res=`eval 'as_val=${'$as_ac_Lib'}
                 echo "$as_val"'`
               { echo "$as_me:$LINENO: result: $ac_res" >&5
echo "$ac_res" >&6; }
as_val=`eval 'as_val=${'$as_ac_Lib'}
                 echo "$as_val"'`
   if test "x$as_val" = x""yes; then
  AST_ALLOGSMAT_FOUND=yes
else
  AST_ALLOGSMAT_FOUND=no
fi
  
      CFLAGS="${ast_ext_lib_check_save_CFLAGS}"
   fi
  
   # now check for the header.
   if test "${AST_ALLOGSMAT_FOUND}" = "yes"; then
      ALLOGSMAT_LIB="${pbxlibdir} -lallogsmat "
      # if --with-allogsmat=DIR has been specified, use it.
      if test "x${ALLOGSMAT_DIR}" != "x"; then
         ALLOGSMAT_INCLUDE="-I${ALLOGSMAT_DIR}/include"
      fi
      ALLOGSMAT_INCLUDE="${ALLOGSMAT_INCLUDE} "
      if test "xliballogsmat.h" = "x" ; then    # no header, assume found
         ALLOGSMAT_HEADER_FOUND="1"
      else                              # check for the header
         ast_ext_lib_check_saved_CPPFLAGS="${CPPFLAGS}"
         CPPFLAGS="${CPPFLAGS} ${ALLOGSMAT_INCLUDE}"
         if test "${ac_cv_header_liballogsmat_h+set}" = set; then
  { echo "$as_me:$LINENO: checking for liballogsmat.h" >&5
echo $ECHO_N "checking for liballogsmat.h... " >&6; }
if test "${ac_cv_header_liballogsmat_h+set}" = set; then
  echo $ECHO_N "(cached) " >&6
fi
{ echo "$as_me:$LINENO: result: $ac_cv_header_liballogsmat_h" >&5
echo "$ac_cv_header_liballogsmat_h" >&6; }
else
  # Is the header compilable?
{ echo "$as_me:$LINENO: checking liballogsmat.h usability" >&5
echo $ECHO_N "checking liballogsmat.h usability... " >&6; }
cat >conftest.$ac_ext <<_ACEOF
/* confdefs.h.  */
_ACEOF
cat confdefs.h >>conftest.$ac_ext
cat >>conftest.$ac_ext <<_ACEOF
/* end confdefs.h.  */
$ac_includes_default
#include <liballogsmat.h>
_ACEOF
rm -f conftest.$ac_objext
if { (ac_try="$ac_compile"
case "(($ac_try" in
  *\"* | *\`* | *\\*) ac_try_echo=\$ac_try;;
  *) ac_try_echo=$ac_try;;
esac
eval ac_try_echo="\"\$as_me:$LINENO: $ac_try_echo\""
echo "$ac_try_echo") >&5
  (eval "$ac_compile") 2>conftest.er1
  ac_status=$?
  grep -v '^ *+' conftest.er1 >conftest.err
  rm -f conftest.er1
  cat conftest.err >&5
  echo "$as_me:$LINENO: \$? = $ac_status" >&5
  (exit $ac_status); } && {
         test -z "$ac_c_werror_flag" ||
         test ! -s conftest.err
       } && test -s conftest.$ac_objext; then
  ac_header_compiler=yes
else
  echo "$as_me: failed program was:" >&5 
sed 's/^/| /' conftest.$ac_ext >&5
 
        ac_header_compiler=no
fi

rm -f core conftest.err conftest.$ac_objext conftest.$ac_ext
{ echo "$as_me:$LINENO: result: $ac_header_compiler" >&5
echo "$ac_header_compiler" >&6; }

# Is the header present?
{ echo "$as_me:$LINENO: checking liballogsmat.h presence" >&5
echo $ECHO_N "checking liballogsmat.h presence... " >&6; }
cat >conftest.$ac_ext <<_ACEOF
/* confdefs.h.  */
_ACEOF
cat confdefs.h >>conftest.$ac_ext
cat >>conftest.$ac_ext <<_ACEOF
/* end confdefs.h.  */
#include <liballogsmat.h>
_ACEOF
if { (ac_try="$ac_cpp conftest.$ac_ext"
case "(($ac_try" in
  *\"* | *\`* | *\\*) ac_try_echo=\$ac_try;;
  *) ac_try_echo=$ac_try;;
esac
eval ac_try_echo="\"\$as_me:$LINENO: $ac_try_echo\""
echo "$ac_try_echo") >&5
  (eval "$ac_cpp conftest.$ac_ext") 2>conftest.er1
  ac_status=$?
  grep -v '^ *+' conftest.er1 >conftest.err
  rm -f conftest.er1
  cat conftest.err >&5
  echo "$as_me:$LINENO: \$? = $ac_status" >&5
  (exit $ac_status); } >/dev/null && {
         test -z "$ac_c_preproc_warn_flag$ac_c_werror_flag" ||
         test ! -s conftest.err
       }; then
  ac_header_preproc=yes
else
  echo "$as_me: failed program was:" >&5
sed 's/^/| /' conftest.$ac_ext >&5

  ac_header_preproc=no
fi

rm -f conftest.err conftest.$ac_ext
{ echo "$as_me:$LINENO: result: $ac_header_preproc" >&5
echo "$ac_header_preproc" >&6; }

# So?  What about this header?
case $ac_header_compiler:$ac_header_preproc:$ac_c_preproc_warn_flag in
  yes:no: )
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h: accepted by the compiler, rejected by the preprocessor!" >&5
echo "$as_me: WARNING: liballogsmat.h: accepted by the compiler, rejected by the preprocessor!" >&2;}
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h: proceeding with the compiler's result" >&5
echo "$as_me: WARNING: liballogsmat.h: proceeding with the compiler's result" >&2;}
    ac_header_preproc=yes
    ;;
  no:yes:* )
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h: present but cannot be compiled" >&5
echo "$as_me: WARNING: liballogsmat.h: present but cannot be compiled" >&2;}
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h:     check for missing prerequisite headers?" >&5
echo "$as_me: WARNING: liballogsmat.h:     check for missing prerequisite headers?" >&2;}
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h: see the Autoconf documentation" >&5
echo "$as_me: WARNING: liballogsmat.h: see the Autoconf documentation" >&2;}
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h:     section \"Present But Cannot Be Compiled\"" >&5
echo "$as_me: WARNING: liballogsmat.h:     section \"Present But Cannot Be Compiled\"" >&2;}
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h: proceeding with the preprocessor's result" >&5
echo "$as_me: WARNING: liballogsmat.h: proceeding with the preprocessor's result" >&2;}
    { echo "$as_me:$LINENO: WARNING: liballogsmat.h: in the future, the compiler will take precedence" >&5
echo "$as_me: WARNING: liballogsmat.h: in the future, the compiler will take precedence" >&2;}
    ( cat <<\_ASBOX
## ------------------------------------------ ##
## Report this to https://issues.asterisk.org ##
## ------------------------------------------ ##
_ASBOX
     ) | sed "s/^/$as_me: WARNING:     /" >&2
    ;;
esac
{ echo "$as_me:$LINENO: checking for liballogsmat.h" >&5
echo $ECHO_N "checking for liballogsmat.h... " >&6; }
if test "${ac_cv_header_liballogsmat_h+set}" = set; then
  echo $ECHO_N "(cached) " >&6
else
  ac_cv_header_liballogsmat_h=$ac_header_preproc
fi
{ echo "$as_me:$LINENO: result: $ac_cv_header_liballogsmat_h" >&5
echo "$ac_cv_header_liballogsmat_h" >&6; }
    
fi
if test "x$ac_cv_header_liballogsmat_h" = x""yes; then
  ALLOGSMAT_HEADER_FOUND=1
else
  ALLOGSMAT_HEADER_FOUND=0
fi


         CPPFLAGS="${ast_ext_lib_check_saved_CPPFLAGS}"
      fi
      if test "x${ALLOGSMAT_HEADER_FOUND}" = "x0" ; then
         ALLOGSMAT_LIB=""
         ALLOGSMAT_INCLUDE=""
      else
         if test "x${pbxfuncname}" = "x" ; then         # only checking headers -> no library
            ALLOGSMAT_LIB=""
         fi
         PBX_ALLOGSMAT=1
         cat >>confdefs.h <<_ACEOF
#define HAVE_ALLOGSMAT 1
_ACEOF

      fi
   fi
fi













