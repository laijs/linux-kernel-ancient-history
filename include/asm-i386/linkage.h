#ifndef __ASM_LINKAGE_H
#define __ASM_LINKAGE_H

#define asmlinkage CPP_ASMLINKAGE __attribute__((regparm(0)))
#define FASTCALL(x)	x __attribute__((regparm(3)))

#ifdef CONFIG_X86_ALIGNMENT_16
#define __ALIGN .align 16,0x90
#endif

#endif
