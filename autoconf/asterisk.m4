dnl FILE: asterisk.m4
dnl COPYRIGHT: http://chan-sccp.github.io/chan-sccp/ group 2009
dnl CREATED BY: Created by Diederik de Groot
dnl LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
dnl          See the LICENSE file at the top of the source tree.

dnl Determine the Asterisk major version and refuse anything older than Asterisk 20.
dnl
dnl Asterisk does not export its version number in a header (asterisk/version.h is a stub since 1.8), but
dnl AMI_VERSION in asterisk/manager.h is bumped with every major release: AMI major = Asterisk major - 11
dnl (Asterisk 20 -> "9.0.0", 21 -> "10.0.0", 22 -> "11.0.0"). The pkg-config version is used as a cross check
dnl and to pick up the minor version. --with-asterisk-version=MAJOR[.MINOR] overrides detection.
dnl
dnl Defines: ASTERISK_VERSION_MAJOR (e.g. 22), ASTERISK_VERSION_GROUP (100 + major, e.g. 122, kept for legacy
dnl          comparisons) and ASTERISK_VERSION_NUMBER (10000 + major * 100 + minor, e.g. 12205).
AC_DEFUN([AST_GET_VERSION], [
	CONFIGURE_PART([Checking Asterisk Version:])
	AC_ARG_WITH([asterisk-version],
		[AS_HELP_STRING([--with-asterisk-version=MAJOR[[.MINOR]]], [override the detected Asterisk version (e.g. 22 or 22.5)])],
		[cs_ast_version_override="${withval}"],
		[cs_ast_version_override="no"])

	cs_ast_header_include="
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
#define AST_MODULE_SELF_SYM __internal_chan_sccp_la_self
#define AST_MODULE \"chan_sccp\"
#include <asterisk.h>
"
	ASTERISK_VERSION_MAJOR=""
	ASTERISK_VERSION_MINOR=""

	dnl minor version from pkg-config (Debian style "1:22.5.2~dfsg+..." -> 22 / 5)
	cs_pc_version=`echo "${PBX_VERSION}" | ${SED} -e 's/^[[0-9]]*://' -e 's/[[^0-9.]].*$//'`
	cs_pc_major=`echo "${cs_pc_version}" | cut -d. -f1`
	cs_pc_minor=`echo "${cs_pc_version}" | cut -s -d. -f2`

	AS_IF([test "x${cs_ast_version_override}" != "xno" && test "x${cs_ast_version_override}" != "xyes"], [
		ASTERISK_VERSION_MAJOR=`echo "${cs_ast_version_override}" | cut -d. -f1`
		ASTERISK_VERSION_MINOR=`echo "${cs_ast_version_override}" | cut -s -d. -f2`
		AC_MSG_NOTICE([Asterisk version ${ASTERISK_VERSION_MAJOR}.${ASTERISK_VERSION_MINOR:-0} given on the command line])
	], [
		AC_CHECK_HEADER([asterisk/ast_version.h], [], [AC_MSG_ERROR([asterisk/ast_version.h not found. chan-sccp needs the development headers of Asterisk >= 20 (asterisk-dev / asterisk-devel, or 'make install-headers').])], [${cs_ast_header_include}])
		AC_MSG_CHECKING([Asterisk major version (AMI_VERSION in asterisk/manager.h)])
		dnl locate the manager.h the compiler really uses (via the include trace) and read the define from the file
		dnl (expanding the macro through the preprocessor is not reliable with every compiler)
		cat > conftest.$ac_ext <<_ACEOF
${cs_ast_header_include}
#include <asterisk/manager.h>
_ACEOF
		cs_manager_h=`(eval "$ac_cpp -H conftest.$ac_ext") 2>&1 >/dev/null | ${SED} -n 's/^\.[[. ]]*\(.*asterisk\/manager\.h\)[[ 	]]*$/\1/p' | head -n1`
		rm -f conftest.$ac_ext conftest.$ac_objext
		AS_IF([test -z "${cs_manager_h}" || test ! -r "${cs_manager_h}"], [
			AS_IF([test -r "${PBX_INCLUDE}/manager.h"], [cs_manager_h="${PBX_INCLUDE}/manager.h"], [cs_manager_h="${PBX_INCLUDE}/asterisk/manager.h"])
		])
		dnl note: '@%:@' is the autoconf quadrigraph for '#', which would otherwise start an m4 comment
		cs_ami_major=`${SED} -n 's/^@%:@[[ 	]]*define[[ 	]]*AMI_VERSION[[ 	]]*"\([[0-9]][[0-9]]*\)\..*$/\1/p' "${cs_manager_h}" 2>/dev/null | head -n1`
		AS_IF([test -z "${cs_ami_major}"], [
			AC_MSG_RESULT([unknown])
			AC_MSG_ERROR([could not determine the Asterisk version from asterisk/manager.h. Use --with-asterisk-version=MAJOR.MINOR to specify it.])
		])
		ASTERISK_VERSION_MAJOR=`expr ${cs_ami_major} + 11`
		ASTERISK_VERSION_MINOR="${cs_pc_minor}"
		AC_MSG_RESULT([${ASTERISK_VERSION_MAJOR} (AMI ${cs_ami_major})])
		AS_IF([test -n "${cs_pc_major}" && test "x${cs_pc_major}" != "x${ASTERISK_VERSION_MAJOR}"], [
			AC_MSG_WARN([pkg-config reports Asterisk ${cs_pc_major} but the headers belong to Asterisk ${ASTERISK_VERSION_MAJOR}; trusting the headers. Use --with-asterisk-version to override.])
		])
	])
	ASTERISK_VERSION_MINOR="${ASTERISK_VERSION_MINOR:-0}"

	AS_IF([test "${ASTERISK_VERSION_MAJOR}" -lt 20 2>/dev/null || test -z "${ASTERISK_VERSION_MAJOR}"], [
		AC_MSG_ERROR([Asterisk ${ASTERISK_VERSION_MAJOR} detected. This version of chan-sccp requires Asterisk 20 or newer (20, 21, 22, ...). For older Asterisk releases use chan-sccp v4.3.x.])
	])

	ASTERISK_VERSION_GROUP=`expr 100 + ${ASTERISK_VERSION_MAJOR}`
	ASTERISK_VERSION_NUMBER=`expr 10000 + ${ASTERISK_VERSION_MAJOR} \* 100 + ${ASTERISK_VERSION_MINOR}`
	ASTERISK_VER_GROUP="${ASTERISK_VERSION_GROUP}"

	AC_DEFINE_UNQUOTED([ASTERISK_VERSION_MAJOR], [${ASTERISK_VERSION_MAJOR}], [Asterisk major version (e.g. 22)])
	AC_DEFINE_UNQUOTED([ASTERISK_VERSION_GROUP], [${ASTERISK_VERSION_GROUP}], [Asterisk version group: 100 + major (e.g. 122)])
	AC_DEFINE_UNQUOTED([ASTERISK_VERSION_NUMBER], [${ASTERISK_VERSION_NUMBER}], [Asterisk version number: 10000 + major * 100 + minor (e.g. 12205), legacy encoding])
	AC_SUBST([ASTERISK_VERSION_MAJOR])
	AC_SUBST([ASTERISK_VERSION_GROUP])
	AC_SUBST([ASTERISK_VERSION_NUMBER])
	AC_MSG_NOTICE([Building for Asterisk ${ASTERISK_VERSION_MAJOR}.${ASTERISK_VERSION_MINOR}])
])

dnl Check the Asterisk headers and the build options Asterisk was compiled with.
dnl
dnl Everything that only depends on the Asterisk *version* is known for Asterisk >= 20 and is defined
dnl unconditionally in src/pbx_impl/ast/define.h. Only properties that depend on how the installed
dnl Asterisk was built (menuselect options) are probed here.
AC_DEFUN([AST_CHECK_HEADERS],[
	CONFIGURE_PART([Checking Asterisk Headers:])

	CFLAGS_backup="${CFLAGS}"
	CFLAGS="${CFLAGS_saved}"
	AX_APPEND_COMPILE_FLAGS([-Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=int-conversion], TEST_SUPPORTED_CFLAGS)
	CFLAGS="${CFLAGS_saved} ${TEST_SUPPORTED_CFLAGS}"

	HEADER_INCLUDE="
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
#define AST_MODULE_SELF_SYM __internal_chan_sccp_la_self
#define AST_MODULE \"chan_sccp\"
#include <asterisk.h>
#include <asterisk/autoconfig.h>
#include <asterisk/buildopts.h>
"
	dnl headers chan-sccp cannot live without; missing ones indicate an incomplete asterisk-dev installation
	AC_CHECK_HEADERS([asterisk.h asterisk/autoconfig.h asterisk/buildopts.h asterisk/lock.h asterisk/channel.h asterisk/pbx.h asterisk/bridge.h asterisk/rtp_engine.h asterisk/format_cache.h asterisk/stasis.h asterisk/mwi.h asterisk/pickup.h asterisk/parking.h asterisk/manager.h asterisk/http.h asterisk/backtrace.h asterisk/vector.h],
		[],
		[AC_MSG_ERROR([required Asterisk header '$ac_header' not found. Install the Asterisk >= 20 development headers (asterisk-dev / asterisk-devel or 'make install-headers').])],
		[${HEADER_INCLUDE}])

	dnl basic asterisk types used throughout chan-sccp
	AC_DEFINE([PBX_CHANNEL_TYPE],[struct ast_channel],[Define PBX_CHANNEL_TYPE as 'struct ast_channel'])
	AC_DEFINE([PBX_VARIABLE_TYPE],[struct ast_variable],[Define PBX_VARIABLE_TYPE as 'struct ast_variable'])
	AC_DEFINE([PBX_FRAME_TYPE],[struct ast_frame],[Define PBX_FRAME_TYPE as 'struct ast_frame'])
	AC_DEFINE([PBX_RTP_TYPE],[struct ast_rtp_instance],[Define PBX_RTP_TYPE as 'struct ast_rtp_instance'])
	HAVE_PBX_HTTP=yes
	AC_SUBST([HAVE_PBX_HTTP])

	dnl menuselect build options of the installed asterisk
	AC_MSG_CHECKING([ - if asterisk was compiled with HAVE_BKTR])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(HAVE_BKTR)
			yes
		#endif
	], [
		AC_DEFINE([HAVE_BKTR],1,[asterisk compiled with backtrace support])
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
	])

	AC_MSG_CHECKING([ - if asterisk was compiled with LOW_MEMORY])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(LOW_MEMORY)
			yes
		#endif
	], [
		AC_DEFINE([CS_LOW_MEMORY],1,[asterisk compiled with 'LOW_MEMORY'])
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
	])

	AC_MSG_CHECKING([ - if asterisk was compiled with TEST_FRAMEWORK])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(TEST_FRAMEWORK)
			yes
		#endif
	], [
		AC_DEFINE([CS_TEST_FRAMEWORK],1,[asterisk compiled with 'TEST_FRAMEWORK'])
		TEST_FRAMEWORK=yes
		AC_MSG_RESULT(yes)
	], [
		TEST_FRAMEWORK=no
		AC_MSG_RESULT(no)
	])
	AC_SUBST([TEST_FRAMEWORK])

	SANITIZE_CFLAGS=""
	SANITIZE_LDFLAGS=""
	AC_MSG_CHECKING([ - if asterisk was compiled with ADDRESS_SANITIZER])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(ADDRESS_SANITIZER)
			yes
		#endif
	], [
		SANITIZE_CFLAGS="-fsanitize=address"
		SANITIZE_LDFLAGS="-fno-omit-frame-pointer -fsanitize=address"
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
	])
	AC_MSG_CHECKING([ - if asterisk was compiled with THREAD_SANITIZER])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(THREAD_SANITIZER)
			yes
		#endif
	], [
		SANITIZE_CFLAGS="-fsanitize=thread -pie -fPIE"
		SANITIZE_LDFLAGS="-fno-omit-frame-pointer -fsanitize=thread"
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
	])
	AC_MSG_CHECKING([ - if asterisk was compiled with LEAK_SANITIZER])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(LEAK_SANITIZER)
			yes
		#endif
	], [
		SANITIZE_CFLAGS="${SANITIZE_CFLAGS} -fsanitize=leak"
		SANITIZE_LDFLAGS="${SANITIZE_LDFLAGS} -fno-omit-frame-pointer -fsanitize=leak"
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
	])
	AC_MSG_CHECKING([ - if asterisk was compiled with UNDEFINED_SANITIZER])
	AC_EGREP_CPP([yes], [
		${HEADER_INCLUDE}
		#if defined(UNDEFINED_SANITIZER)
			yes
		#endif
	], [
		SANITIZE_CFLAGS="${SANITIZE_CFLAGS} -fsanitize=undefined"
		SANITIZE_LDFLAGS="${SANITIZE_LDFLAGS} -fno-omit-frame-pointer -fsanitize=undefined"
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
	])
	AC_SUBST([SANITIZE_CFLAGS])
	AC_SUBST([SANITIZE_LDFLAGS])

	dnl restore previous CFLAGS from backup
	CFLAGS="${CFLAGS_backup}"
])
