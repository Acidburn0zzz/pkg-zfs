AC_DEFUN([ZFS_AC_LICENSE], [
	AC_MSG_CHECKING([zfs license])
	LICENSE=`grep MODULE_LICENSE module/zfs/zfs_ioctl.c | cut -f2 -d'"'`
	AC_MSG_RESULT([$LICENSE])
	if test "$LICENSE" = GPL; then
		AC_DEFINE([HAVE_GPL_ONLY_SYMBOLS], [1],
		          [Define to 1 if module is licensed under the GPL])
	fi

	AC_SUBST(LICENSE)
])

AC_DEFUN([ZFS_AC_DEBUG], [
	AC_MSG_CHECKING([whether debugging is enabled])
	AC_ARG_ENABLE( [debug],
		AS_HELP_STRING([--enable-debug],
		[Enable generic debug support (default off)]),
		[ case "$enableval" in
			yes) zfs_ac_debug=yes ;;
			no)  zfs_ac_debug=no  ;;
			*) AC_MSG_RESULT([Error!])
			AC_MSG_ERROR([Bad value "$enableval" for --enable-debug]) ;;
		esac ]
)
if test "$zfs_ac_debug" = yes; then
	AC_MSG_RESULT([yes])
		AC_DEFINE([DEBUG], [1],
		[Define to 1 to enable debug tracing])
		KERNELCPPFLAGS="${KERNELCPPFLAGS} -DDEBUG "
		HOSTCFLAGS="${HOSTCFLAGS} -DDEBUG "
		USERDEBUG="-DDEBUG"
	else
		AC_MSG_RESULT([no])
		AC_DEFINE([NDEBUG], [1],
		[Define to 1 to disable debug tracing])
		KERNELCPPFLAGS="${KERNELCPPFLAGS} -DNDEBUG "
		HOSTCFLAGS="${HOSTCFLAGS} -DNDEBUG "
		USERDEBUG="-DNDEBUG"
	fi

	AC_SUBST(USERDEBUG)
])

AC_DEFUN([ZFS_AC_CONFIG_SCRIPT], [
	cat >.script-config <<EOF
KERNELSRC=${LINUX}
KERNELBUILD=${LINUX_OBJ}
KERNELSRCVER=${LINUX_VERSION}
KERNELMOD=/lib/modules/\${KERNELSRCVER}/kernel

SPLSRC=${SPL}
SPLBUILD=${SPL_OBJ}
SPLSRCVER=${SPL_VERSION}

TOPDIR=${TOPDIR}
BUILDDIR=${BUILDDIR}
LIBDIR=${LIBDIR}
CMDDIR=${CMDDIR}
MODDIR=${MODDIR}
SCRIPTDIR=${SCRIPTDIR}
UDEVDIR=\${TOPDIR}/scripts/udev-rules
ZPOOLDIR=\${TOPDIR}/scripts/zpool-config
ZPIOSDIR=\${TOPDIR}/scripts/zpios-test
ZPIOSPROFILEDIR=\${TOPDIR}/scripts/zpios-profile

ZDB=\${CMDDIR}/zdb/zdb
ZFS=\${CMDDIR}/zfs/zfs
ZINJECT=\${CMDDIR}/zinject/zinject
ZPOOL=\${CMDDIR}/zpool/zpool
ZTEST=\${CMDDIR}/ztest/ztest
ZPIOS=\${CMDDIR}/zpios/zpios

COMMON_SH=\${SCRIPTDIR}/common.sh
ZFS_SH=\${SCRIPTDIR}/zfs.sh
ZPOOL_CREATE_SH=\${SCRIPTDIR}/zpool-create.sh
ZPIOS_SH=\${SCRIPTDIR}/zpios.sh
ZPIOS_SURVEY_SH=\${SCRIPTDIR}/zpios-survey.sh

LDMOD=/sbin/insmod

KERNEL_MODULES=(                                      \\
        \${KERNELMOD}/lib/zlib_deflate/zlib_deflate.ko \\
)

SPL_MODULES=(                                         \\
        \${SPLBUILD}/spl/spl.ko                        \\
)

ZFS_MODULES=(                                         \\
        \${MODDIR}/avl/zavl.ko                         \\
        \${MODDIR}/nvpair/znvpair.ko                   \\
        \${MODDIR}/unicode/zunicode.ko                 \\
        \${MODDIR}/zcommon/zcommon.ko                  \\
        \${MODDIR}/zfs/zfs.ko                          \\
)

ZPIOS_MODULES=(                                       \\
        \${MODDIR}/zpios/zpios.ko                      \\
)

MODULES=(                                             \\
        \${KERNEL_MODULES[[*]]}                          \\
        \${SPL_MODULES[[*]]}                             \\
        \${ZFS_MODULES[[*]]}                             \\
)
EOF
])

AC_DEFUN([ZFS_AC_CONFIG], [
	TOPDIR=`readlink -f ${srcdir}`
	BUILDDIR=$TOPDIR
	LIBDIR=$TOPDIR/lib
	CMDDIR=$TOPDIR/cmd
	MODDIR=$TOPDIR/module
	SCRIPTDIR=$TOPDIR/scripts
	TARGET_ASM_DIR=asm-generic

	AC_SUBST(TOPDIR)
	AC_SUBST(BUILDDIR)
	AC_SUBST(LIBDIR)
	AC_SUBST(CMDDIR)
	AC_SUBST(MODDIR)
	AC_SUBST(SCRIPTDIR)
	AC_SUBST(TARGET_ASM_DIR)

	ZFS_CONFIG=all
	AC_ARG_WITH([config],
		AS_HELP_STRING([--with-config=CONFIG],
		[Config file 'kernel|user|all|srpm']),
		[ZFS_CONFIG="$withval"])

	AC_MSG_CHECKING([zfs config])
	AC_MSG_RESULT([$ZFS_CONFIG]);
	AC_SUBST(ZFS_CONFIG)

	case "$ZFS_CONFIG" in
		kernel) ZFS_AC_CONFIG_KERNEL ;;
		user)	ZFS_AC_CONFIG_USER   ;;
		all)    ZFS_AC_CONFIG_KERNEL
			ZFS_AC_CONFIG_USER   ;;
		srpm)                        ;;
		*)
		AC_MSG_RESULT([Error!])
		AC_MSG_ERROR([Bad value "$ZFS_CONFIG" for --with-config,
		              user kernel|user|all|srpm]) ;;
	esac

	AM_CONDITIONAL([CONFIG_USER],
	               [test "$ZFS_CONFIG" = user] ||
	               [test "$ZFS_CONFIG" = all])
	AM_CONDITIONAL([CONFIG_KERNEL],
	               [test "$ZFS_CONFIG" = kernel] ||
	               [test "$ZFS_CONFIG" = all])

	ZFS_AC_CONFIG_SCRIPT
])

dnl #
dnl # Check for rpm+rpmbuild to build RPM packages.  If these tools
dnl # are missing it is non-fatal but you will not be able to build
dnl # RPM packages and will be warned if you try too.
dnl #
AC_DEFUN([ZFS_AC_RPM], [
	RPM=rpm
	RPMBUILD=rpmbuild

	AC_MSG_CHECKING([whether $RPM is available])
	AS_IF([tmp=$($RPM --version 2>/dev/null)], [
		RPM_VERSION=$(echo $tmp | $AWK '/RPM/ { print $[3] }')
		HAVE_RPM=yes
		AC_MSG_RESULT([$HAVE_RPM ($RPM_VERSION)])
	],[
		HAVE_RPM=no
		AC_MSG_RESULT([$HAVE_RPM])
	])

	AC_MSG_CHECKING([whether $RPMBUILD is available])
	AS_IF([tmp=$($RPMBUILD --version 2>/dev/null)], [
		RPMBUILD_VERSION=$(echo $tmp | $AWK '/RPM/ { print $[3] }')
		HAVE_RPMBUILD=yes
		AC_MSG_RESULT([$HAVE_RPMBUILD ($RPMBUILD_VERSION)])
	],[
		HAVE_RPMBUILD=no
		AC_MSG_RESULT([$HAVE_RPMBUILD])
	])

	AC_SUBST(HAVE_RPM)
	AC_SUBST(RPM)
	AC_SUBST(RPM_VERSION)

	AC_SUBST(HAVE_RPMBUILD)
	AC_SUBST(RPMBUILD)
	AC_SUBST(RPMBUILD_VERSION)
])

dnl #
dnl # Check for dpkg+dpkg-buildpackage to build DEB packages.  If these
dnl # tools are missing it is non-fatal but you will not be able to build
dnl # DEB packages and will be warned if you try too.
dnl #
AC_DEFUN([ZFS_AC_DPKG], [
	DPKG=dpkg
	DPKGBUILD=dpkg-buildpackage

	AC_MSG_CHECKING([whether $DPKG is available])
	AS_IF([tmp=$($DPKG --version 2>/dev/null)], [
		DPKG_VERSION=$(echo $tmp | $AWK '/Debian/ { print $[7] }')
		HAVE_DPKG=yes
		AC_MSG_RESULT([$HAVE_DPKG ($DPKG_VERSION)])
	],[
		HAVE_DPKG=no
		AC_MSG_RESULT([$HAVE_DPKG])
	])

	AC_MSG_CHECKING([whether $DPKGBUILD is available])
	AS_IF([tmp=$($DPKGBUILD --version 2>/dev/null)], [
		DPKGBUILD_VERSION=$(echo $tmp | \
		    $AWK '/Debian/ { print $[4] }' | cut -f-4 -d'.')
		HAVE_DPKGBUILD=yes
		AC_MSG_RESULT([$HAVE_DPKGBUILD ($DPKGBUILD_VERSION)])
	],[
		HAVE_DPKGBUILD=no
		AC_MSG_RESULT([$HAVE_DPKGBUILD])
	])

	AC_SUBST(HAVE_DPKG)
	AC_SUBST(DPKG)
	AC_SUBST(DPKG_VERSION)

	AC_SUBST(HAVE_DPKGBUILD)
	AC_SUBST(DPKGBUILD)
	AC_SUBST(DPKGBUILD_VERSION)
])

dnl #
dnl # Until native packaging for various different packing systems
dnl # can be added the least we can do is attempt to use alien to
dnl # convert the RPM packages to the needed package type.  This is
dnl # a hack but so far it has worked reasonable well.
dnl #
AC_DEFUN([ZFS_AC_ALIEN], [
	ALIEN=alien

	AC_MSG_CHECKING([whether $ALIEN is available])
	AS_IF([tmp=$($ALIEN --version 2>/dev/null)], [
		ALIEN_VERSION=$(echo $tmp | $AWK '{ print $[3] }')
		HAVE_ALIEN=yes
		AC_MSG_RESULT([$HAVE_ALIEN ($ALIEN_VERSION)])
	],[
		HAVE_ALIEN=no
		AC_MSG_RESULT([$HAVE_ALIEN])
	])

	AC_SUBST(HAVE_ALIEN)
	AC_SUBST(ALIEN)
	AC_SUBST(ALIEN_VERSION)
])

dnl #
dnl # Using the VENDOR tag from config.guess set the default
dnl # package type for 'make pkg': (rpm | deb | tgz)
dnl #
AC_DEFUN([ZFS_AC_DEFAULT_PACKAGE], [
	VENDOR=$(echo $ac_build_alias | cut -f2 -d'-')

	AC_MSG_CHECKING([default package type])
	case "$VENDOR" in
		fedora)     DEFAULT_PACKAGE=rpm ;;
		redhat)     DEFAULT_PACKAGE=rpm ;;
		sles)       DEFAULT_PACKAGE=rpm ;;
		ubuntu)     DEFAULT_PACKAGE=deb ;;
		debian)     DEFAULT_PACKAGE=deb ;;
		slackware)  DEFAULT_PACKAGE=tgz ;;
		*)          DEFAULT_PACKAGE=rpm ;;
	esac

	AC_MSG_RESULT([$DEFAULT_PACKAGE])
	AC_SUBST(DEFAULT_PACKAGE)
])

dnl #
dnl # Default ZFS package configuration
dnl #
AC_DEFUN([ZFS_AC_PACKAGE], [
	ZFS_AC_RPM
	ZFS_AC_DPKG
	ZFS_AC_ALIEN
	ZFS_AC_DEFAULT_PACKAGE
])
