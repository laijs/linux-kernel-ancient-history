#ifndef _LINUX_MODULE_H
#define _LINUX_MODULE_H
/*
 * Dynamic loading of modules into the kernel.
 *
 * Rewritten by Richard Henderson <rth@tamu.edu> Dec 1996
 * Rewritten again by Rusty Russell, 2002
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/elf.h>
#include <linux/stat.h>
#include <linux/compiler.h>
#include <linux/cache.h>
#include <linux/kmod.h>
#include <asm/module.h>
#include <asm/uaccess.h> /* For struct exception_table_entry */

/* Not Yet Implemented */
#define MODULE_LICENSE(name)
#define MODULE_AUTHOR(name)
#define MODULE_DESCRIPTION(desc)
#define MODULE_SUPPORTED_DEVICE(name)
#define MODULE_GENERIC_TABLE(gtype,name)
#define MODULE_DEVICE_TABLE(type,name)
#define MODULE_PARM_DESC(var,desc)
#define print_symbol(format, addr)
#define print_modules()

#define MODULE_NAME_LEN (64 - sizeof(unsigned long))
struct kernel_symbol
{
	unsigned long value;
	char name[MODULE_NAME_LEN];
};

#ifdef MODULE
/* This is magically filled in by the linker, but THIS_MODULE must be
   a constant so it works in initializers. */
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE ((struct module *)0)
#endif

#ifdef CONFIG_MODULES
/* Get/put a kernel symbol (calls must be symmetric) */
void *__symbol_get(const char *symbol);
void *__symbol_get_gpl(const char *symbol);
#define symbol_get(x) ((typeof(&x))(__symbol_get(#x)))
#define symbol_put(x) __symbol_put(#x)

/* For every exported symbol, place a struct in the __ksymtab section */
#define EXPORT_SYMBOL(sym)				\
	const struct kernel_symbol __ksymtab_##sym	\
	__attribute__((section("__ksymtab")))		\
	= { (unsigned long)&sym, #sym }

#define EXPORT_SYMBOL_NOVERS(sym) EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym) EXPORT_SYMBOL(sym)

struct kernel_symbol_group
{
	/* Links us into the global symbol list */
	struct list_head list;

	/* Module which owns it (if any) */
	struct module *owner;

	unsigned int num_syms;
	const struct kernel_symbol *syms;
};

struct exception_table
{
	struct list_head list;

	unsigned int num_entries;
	const struct exception_table_entry *entry;
};

struct module_ref
{
	atomic_t count;
} ____cacheline_aligned;

struct module
{
	/* Am I live (yet)? */
	int live;

	/* Member of list of modules */
	struct list_head list;

	/* Unique handle for this module */
	char name[MODULE_NAME_LEN];

	/* Exported symbols */
	struct kernel_symbol_group symbols;

	/* Exception tables */
	struct exception_table extable;

	/* Startup function. */
	int (*init)(void);

	/* If this is non-NULL, vfree after init() returns */
	void *module_init;

	/* Here is the actual code + data, vfree'd on unload. */
	void *module_core;

	/* Here are the sizes of the init and core sections */
	unsigned long init_size, core_size;

	/* Arch-specific module values */
	struct mod_arch_specific arch;

	/* Am I unsafe to unload? */
	int unsafe;

#ifdef CONFIG_MODULE_UNLOAD
	/* Reference counts */
	struct module_ref ref[NR_CPUS];

	/* What modules depend on me? */
	struct list_head modules_which_use_me;

	/* Who is waiting for us to be unloaded */
	struct task_struct *waiter;

	/* Destruction function. */
	void (*exit)(void);
#endif

	/* The command line arguments (may be mangled).  People like
	   keeping pointers to this stuff */
	char args[0];
};

/* Helper function for arch-specific module loaders */
unsigned long find_symbol_internal(Elf_Shdr *sechdrs,
				   unsigned int symindex,
				   const char *strtab,
				   const char *name,
				   struct module *mod,
				   struct kernel_symbol_group **group);

/* These must be implemented by the specific architecture */

/* vmalloc AND zero for the non-releasable code; return ERR_PTR() on error. */
void *module_core_alloc(const Elf_Ehdr *hdr,
			const Elf_Shdr *sechdrs,
			const char *secstrings,
			struct module *mod);

/* vmalloc and zero (if any) for sections to be freed after init.
   Return ERR_PTR() on error. */
void *module_init_alloc(const Elf_Ehdr *hdr,
			const Elf_Shdr *sechdrs,
			const char *secstrings,
			struct module *mod);

/* Apply the given relocation to the (simplified) ELF.  Return -error
   or 0. */
int apply_relocate(Elf_Shdr *sechdrs,
		   const char *strtab,
		   unsigned int symindex,
		   unsigned int relsec,
		   struct module *mod);

/* Apply the given add relocation to the (simplified) ELF.  Return
   -error or 0 */
int apply_relocate_add(Elf_Shdr *sechdrs,
		       const char *strtab,
		       unsigned int symindex,
		       unsigned int relsec,
		       struct module *mod);

/* Any final processing of module before access.  Return -error or 0. */
int module_finalize(const Elf_Ehdr *hdr,
		    const Elf_Shdr *sechdrs,
		    struct module *mod);

/* Free memory returned from module_core_alloc/module_init_alloc */
void module_free(struct module *mod, void *module_region);

#ifdef CONFIG_MODULE_UNLOAD
void __symbol_put(const char *symbol);
void symbol_put_addr(void *addr);

/* We only need protection against local interrupts. */
#ifndef __HAVE_ARCH_LOCAL_INC
#define local_inc(x) atomic_inc(x)
#define local_dec(x) atomic_dec(x)
#endif

static inline int try_module_get(struct module *module)
{
	int ret = 1;

	if (module) {
		unsigned int cpu = get_cpu();
		if (likely(module->live))
			local_inc(&module->ref[cpu].count);
		else
			ret = 0;
		put_cpu();
	}
	return ret;
}

static inline void module_put(struct module *module)
{
	if (module) {
		unsigned int cpu = get_cpu();
		local_dec(&module->ref[cpu].count);
		/* Maybe they're waiting for us to drop reference? */
		if (unlikely(!module->live))
			wake_up_process(module->waiter);
		put_cpu();
	}
}

#else /*!CONFIG_MODULE_UNLOAD*/
static inline int try_module_get(struct module *module)
{
	return !module || module->live;
}
static inline void module_put(struct module *module)
{
}
#define symbol_put(x) do { } while(0)
#define symbol_put_addr(p) do { } while(0)

#endif /* CONFIG_MODULE_UNLOAD */

#define __unsafe(mod)							     \
do {									     \
	if (mod && !(mod)->unsafe) {					     \
		printk(KERN_WARNING					     \
		       "Module %s cannot be unloaded due to unsafe usage in" \
		       " %s:%u\n", (mod)->name, __FILE__, __LINE__);	     \
		(mod)->unsafe = 1;					     \
	}								     \
} while(0)

#else /* !CONFIG_MODULES... */
#define EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)
#define EXPORT_SYMBOL_NOVERS(sym)

/* Get/put a kernel symbol (calls should be symmetric) */
#define symbol_get(x) (&(x))
#define symbol_put(x) do { } while(0)

#define try_module_get(module) 1
#define module_put(module) do { } while(0)

#define __unsafe(mod)
#endif /* CONFIG_MODULES */

/* For archs to search exception tables */
extern struct list_head extables;
extern spinlock_t modlist_lock;

#define symbol_request(x) try_then_request_module(symbol_get(x), "symbol:" #x)

/* BELOW HERE ALL THESE ARE OBSOLETE AND WILL VANISH */
#define __MOD_INC_USE_COUNT(mod) \
	do { __unsafe(mod); (void)try_module_get(mod); } while(0)
#define __MOD_DEC_USE_COUNT(mod) module_put(mod)
#define SET_MODULE_OWNER(dev) ((dev)->owner = THIS_MODULE)

/* People do this inside their init routines, when the module isn't
   "live" yet.  They should no longer be doing that, but
   meanwhile... */
#if defined(CONFIG_MODULE_UNLOAD) && defined(MODULE)
#define MOD_INC_USE_COUNT	\
	do { __unsafe(THIS_MODULE); local_inc(&THIS_MODULE->ref[get_cpu()].count); put_cpu(); } while (0)
#else
#define MOD_INC_USE_COUNT \
	do { __unsafe(THIS_MODULE); (void)try_module_get(THIS_MODULE); } while (0)
#endif
#define MOD_DEC_USE_COUNT module_put(THIS_MODULE)
#define try_inc_mod_count(mod) try_module_get(mod)
#define MODULE_PARM(parm,string)
#define EXPORT_NO_SYMBOLS
extern int module_dummy_usage;
#define GET_USE_COUNT(module) (module_dummy_usage)
#define MOD_IN_USE 0
#define __mod_between(a_start, a_len, b_start, b_len)		\
(((a_start) >= (b_start) && (a_start) <= (b_start)+(b_len))	\
 || ((a_start)+(a_len) >= (b_start)				\
     && (a_start)+(a_len) <= (b_start)+(b_len)))
#define mod_bound(p, n, m)					\
(((m)->module_init						\
  && __mod_between((p),(n),(m)->module_init,(m)->init_size))	\
 || __mod_between((p),(n),(m)->module_core,(m)->core_size))

/* Old-style "I'll just call it init_module and it'll be run at
   insert".  Use module_init(myroutine) instead. */
#ifdef MODULE
/* Used as "int init_module(void) { ... }".  Get funky to insert modname. */
#define init_module(voidarg)						\
	__initfn(void);							\
	char __module_name[] __attribute__((section(".modulename"))) =	\
	__stringify(KBUILD_MODNAME);					\
	int __initfn(void)
#define cleanup_module(voidarg) __exitfn(void)
#endif

/* Use symbol_get and symbol_put instead.  You'll thank me. */
#define HAVE_INTER_MODULE
extern void inter_module_register(const char *, struct module *, const void *);
extern void inter_module_unregister(const char *);
extern const void *inter_module_get(const char *);
extern const void *inter_module_get_request(const char *, const char *);
extern void inter_module_put(const char *);

#endif /* _LINUX_MODULE_H */
