/*
 *  This file is part of the SPL: Solaris Porting Layer.
 *
 *  Copyright (c) 2008 Lawrence Livermore National Security, LLC.
 *  Produced at Lawrence Livermore National Laboratory
 *  Written by:
 *          Brian Behlendorf <behlendorf1@llnl.gov>,
 *          Herb Wartens <wartens2@llnl.gov>,
 *          Jim Garlick <garlick@llnl.gov>
 *  UCRL-CODE-235197
 *
 *  This is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/vmsystm.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/taskq.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/kstat.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <linux/kmod.h>
#include <sys/tsd_hashtable.h>
#include "spl_config.h"

#ifdef DEBUG_SUBSYSTEM
#undef DEBUG_SUBSYSTEM
#endif

#define DEBUG_SUBSYSTEM S_GENERIC
extern struct tsd_hash_table *tsd_hash_table;

char spl_version[16] = "SPL v" SPL_META_VERSION;

long spl_hostid = 0;
EXPORT_SYMBOL(spl_hostid);

char hw_serial[HW_HOSTID_LEN] = "<none>";
EXPORT_SYMBOL(hw_serial);

int p0 = 0;
EXPORT_SYMBOL(p0);

#ifndef HAVE_KALLSYMS_LOOKUP_NAME
kallsyms_lookup_name_t spl_kallsyms_lookup_name_fn = SYMBOL_POISON;
#endif

int
highbit(unsigned long i)
{
        register int h = 1;
        ENTRY;

        if (i == 0)
                RETURN(0);
#if BITS_PER_LONG == 64
        if (i & 0xffffffff00000000ul) {
                h += 32; i >>= 32;
        }
#endif
        if (i & 0xffff0000) {
                h += 16; i >>= 16;
        }
        if (i & 0xff00) {
                h += 8; i >>= 8;
        }
        if (i & 0xf0) {
                h += 4; i >>= 4;
        }
        if (i & 0xc) {
                h += 2; i >>= 2;
        }
        if (i & 0x2) {
                h += 1;
        }
        RETURN(h);
}
EXPORT_SYMBOL(highbit);

/*
 * Implementation of 64 bit division for 32-bit machines.
 */
#if BITS_PER_LONG == 32
uint64_t __udivdi3(uint64_t dividend, uint64_t divisor)
{
#if defined(HAVE_DIV64_64) /* 2.6.22 - 2.6.25 API */
	return div64_64(dividend, divisor);
#elif defined(HAVE_DIV64_U64) /* 2.6.26 - 2.6.x API */
	return div64_u64(dividend, divisor);
#else
	/* Implementation from 2.6.30 kernel */
	uint32_t high, d;

	high = divisor >> 32;
	if (high) {
		unsigned int shift = fls(high);

		d = divisor >> shift;
		dividend >>= shift;
	} else
		d = divisor;

	return do_div(dividend, d);
#endif /* HAVE_DIV64_64, HAVE_DIV64_U64 */
}
EXPORT_SYMBOL(__udivdi3);

/*
 * Implementation of 64 bit modulo for 32-bit machines.
 */
uint64_t __umoddi3(uint64_t dividend, uint64_t divisor)
{
	return dividend - divisor * (dividend / divisor);
}
EXPORT_SYMBOL(__umoddi3);
#endif /* BITS_PER_LONG */

/* NOTE: The strtoxx behavior is solely based on my reading of the Solaris
 * ddi_strtol(9F) man page.  I have not verified the behavior of these
 * functions against their Solaris counterparts.  It is possible that I
 * may have misinterpreted the man page or the man page is incorrect.
 */
int ddi_strtoul(const char *, char **, int, unsigned long *);
int ddi_strtol(const char *, char **, int, long *);
int ddi_strtoull(const char *, char **, int, unsigned long long *);
int ddi_strtoll(const char *, char **, int, long long *);

#define define_ddi_strtoux(type, valtype)				\
int ddi_strtou##type(const char *str, char **endptr,			\
		     int base, valtype *result)				\
{									\
	valtype last_value, value = 0;					\
	char *ptr = (char *)str;					\
	int flag = 1, digit;						\
									\
	if (strlen(ptr) == 0)						\
		return EINVAL;						\
									\
	/* Auto-detect base based on prefix */				\
	if (!base) {							\
		if (str[0] == '0') {					\
			if (tolower(str[1])=='x' && isxdigit(str[2])) {	\
				base = 16; /* hex */			\
				ptr += 2;				\
			} else if (str[1] >= '0' && str[1] < 8) {	\
				base = 8; /* octal */			\
				ptr += 1;				\
			} else {					\
				return EINVAL;				\
			}						\
		} else {						\
			base = 10; /* decimal */			\
		}							\
	}								\
									\
	while (1) {							\
		if (isdigit(*ptr))					\
			digit = *ptr - '0';				\
		else if (isalpha(*ptr))					\
			digit = tolower(*ptr) - 'a' + 10;		\
		else							\
			break;						\
									\
		if (digit >= base)					\
			break;						\
									\
		last_value = value;					\
		value = value * base + digit;				\
		if (last_value > value) /* Overflow */			\
			return ERANGE;					\
									\
		flag = 1;						\
		ptr++;							\
	}								\
									\
	if (flag)							\
		*result = value;					\
									\
	if (endptr)							\
		*endptr = (char *)(flag ? ptr : str);			\
									\
	return 0;							\
}									\

#define define_ddi_strtox(type, valtype)				\
int ddi_strto##type(const char *str, char **endptr,			\
		       int base, valtype *result)			\
{									\
	int rc;								\
									\
	if (*str == '-') {						\
		rc = ddi_strtou##type(str + 1, endptr, base, result);	\
		if (!rc) {						\
			if (*endptr == str + 1)				\
				*endptr = (char *)str;			\
			else						\
				*result = -*result;			\
		}							\
	} else {							\
		rc = ddi_strtou##type(str, endptr, base, result);	\
	}								\
									\
	return rc;							\
}

define_ddi_strtoux(l, unsigned long)
define_ddi_strtox(l, long)
define_ddi_strtoux(ll, unsigned long long)
define_ddi_strtox(ll, long long)

EXPORT_SYMBOL(ddi_strtoul);
EXPORT_SYMBOL(ddi_strtol);
EXPORT_SYMBOL(ddi_strtoll);
EXPORT_SYMBOL(ddi_strtoull);

int
ddi_copyin(const void *from, void *to, size_t len, int flags)
{
	/* Fake ioctl() issued by kernel, 'from' is a kernel address */
	if (flags & FKIOCTL) {
		memcpy(to, from, len);
		return 0;
	}

	return copyin(from, to, len);
}
EXPORT_SYMBOL(ddi_copyin);

int
ddi_copyout(const void *from, void *to, size_t len, int flags)
{
	/* Fake ioctl() issued by kernel, 'from' is a kernel address */
	if (flags & FKIOCTL) {
		memcpy(to, from, len);
		return 0;
	}

	return copyout(from, to, len);
}
EXPORT_SYMBOL(ddi_copyout);

#ifndef HAVE_PUT_TASK_STRUCT
/*
 * This is only a stub function which should never be used.  The SPL should
 * never be putting away the last reference on a task structure so this will
 * not be called.  However, we still need to define it so the module does not
 * have undefined symbol at load time.  That all said if this impossible
 * thing does somehow happen SBUG() immediately so we know about it.
 */
void
__put_task_struct(struct task_struct *t)
{
	SBUG();
}
EXPORT_SYMBOL(__put_task_struct);
#endif /* HAVE_PUT_TASK_STRUCT */

struct new_utsname *__utsname(void)
{
#ifdef HAVE_INIT_UTSNAME
	return init_utsname();
#else
	return &system_utsname;
#endif
}
EXPORT_SYMBOL(__utsname);

static int
set_hostid(void)
{
	char sh_path[] = "/bin/sh";
	char *argv[] = { sh_path,
	                 "-c",
	                 "/usr/bin/hostid >/proc/sys/kernel/spl/hostid",
	                 NULL };
	char *envp[] = { "HOME=/",
	                 "TERM=linux",
	                 "PATH=/sbin:/usr/sbin:/bin:/usr/bin",
	                 NULL };
	int rc;

	/* Doing address resolution in the kernel is tricky and just
	 * not a good idea in general.  So to set the proper 'hw_serial'
	 * use the usermodehelper support to ask '/bin/sh' to run
	 * '/usr/bin/hostid' and redirect the result to /proc/sys/spl/hostid
	 * for us to use.  It's a horrific solution but it will do for now.
	 */
	rc = call_usermodehelper(sh_path, argv, envp, 1);
	if (rc)
		printk("SPL: Failed user helper '%s %s %s', rc = %d\n",
		       argv[0], argv[1], argv[2], rc);

	return rc;
}

uint32_t
zone_get_hostid(void *zone)
{
	unsigned long hostid;

	/* Only the global zone is supported */
	ASSERT(zone == NULL);

	if (ddi_strtoul(hw_serial, NULL, HW_HOSTID_LEN-1, &hostid) != 0)
		return HW_INVALID_HOSTID;

	return (uint32_t)hostid;
}
EXPORT_SYMBOL(zone_get_hostid);

#ifndef HAVE_KALLSYMS_LOOKUP_NAME
/*
 * Because kallsyms_lookup_name() is no longer exported in the
 * mainline kernel we are forced to resort to somewhat drastic
 * measures.  This function replaces the functionality by performing
 * an upcall to user space where /proc/kallsyms is consulted for
 * the requested address.
 */
#define GET_KALLSYMS_ADDR_CMD						\
	"awk '{ if ( $3 == \"kallsyms_lookup_name\") { print $1 } }' "	\
	"/proc/kallsyms >/proc/sys/kernel/spl/kallsyms_lookup_name"

static int
set_kallsyms_lookup_name(void)
{
	char sh_path[] = "/bin/sh";
	char *argv[] = { sh_path,
	                 "-c",
			 GET_KALLSYMS_ADDR_CMD,
	                 NULL };
	char *envp[] = { "HOME=/",
	                 "TERM=linux",
	                 "PATH=/sbin:/usr/sbin:/bin:/usr/bin",
	                 NULL };
	int rc;

	rc = call_usermodehelper(sh_path, argv, envp, 1);
	if (rc)
		printk("SPL: Failed user helper '%s %s %s', rc = %d\n",
		       argv[0], argv[1], argv[2], rc);

	return rc;
}
#endif

static int
__init spl_init(void)
{
	int rc = 0;

	if ((rc = debug_init()))
		return rc;

	if ((rc = spl_kmem_init()))
		GOTO(out1, rc);

	if ((rc = spl_mutex_init()))
		GOTO(out2, rc);

	if ((rc = spl_rw_init()))
		GOTO(out3, rc);

	if ((rc = spl_taskq_init()))
		GOTO(out4, rc);

	if ((rc = vn_init()))
		GOTO(out5, rc);

	if ((rc = proc_init()))
		GOTO(out6, rc);

	if ((rc = kstat_init()))
		GOTO(out7, rc);

	if ((rc = set_hostid()))
		GOTO(out8, rc = -EADDRNOTAVAIL);

#ifndef HAVE_KALLSYMS_LOOKUP_NAME
	if ((rc = set_kallsyms_lookup_name()))
		GOTO(out8, rc = -EADDRNOTAVAIL);
#endif /* HAVE_KALLSYMS_LOOKUP_NAME */

	if ((rc = spl_kmem_init_kallsyms_lookup()))
		GOTO(out8, rc);

	printk("SPL: Loaded Solaris Porting Layer v%s\n", SPL_META_VERSION);
	RETURN(rc);
out8:
	kstat_fini();
out7:
	proc_fini();
out6:
	vn_fini();
out5:
	spl_taskq_fini();
out4:
	spl_rw_fini();
out3:
	spl_mutex_fini();
out2:
	spl_kmem_fini();
out1:
	debug_fini();

	printk("SPL: Failed to Load Solaris Porting Layer v%s, "
	       "rc = %d\n", SPL_META_VERSION, rc);
	return rc;
}

static void
spl_fini(void)
{
	ENTRY;

	printk("SPL: Unloaded Solaris Porting Layer v%s\n", SPL_META_VERSION);
	/* Remove the TSD's hash table */
	fini_tsd_hash_table(tsd_hash_table);
	kstat_fini();
	proc_fini();
	vn_fini();
	spl_taskq_fini();
	spl_rw_fini();
	spl_mutex_fini();
	spl_kmem_fini();
	debug_fini();
}

/* Called when a dependent module is loaded */
void
spl_setup(void)
{
        /*
         * At module load time the pwd is set to '/' on a Solaris system.
         * On a Linux system will be set to whatever directory the caller
         * was in when executing insmod/modprobe.
         */
        vn_set_pwd("/");
}
EXPORT_SYMBOL(spl_setup);

/* Called when a dependent module is unloaded */
void
spl_cleanup(void)
{
}
EXPORT_SYMBOL(spl_cleanup);

module_init(spl_init);
module_exit(spl_fini);

MODULE_AUTHOR("Lawrence Livermore National Labs");
MODULE_DESCRIPTION("Solaris Porting Layer");
MODULE_LICENSE("GPL");
