#ifndef __ASM_MACH_APIC_H
#define __ASM_MACH_APIC_H

#include <mach_apicdef.h>

#define APIC_DFR_VALUE	(APIC_DFR_FLAT)

#define no_balance_irq (0)
#define esr_disable (0)

#define INT_DELIVERY_MODE dest_LowestPrio
#define INT_DEST_MODE 1     /* logical delivery broadcast to all procs */

#ifdef CONFIG_SMP
 #define TARGET_CPUS cpu_online_map
#else
 #define TARGET_CPUS cpumask_of_cpu(0)
#endif

#define APIC_BROADCAST_ID      0x0F
#define check_apicid_used(bitmap, apicid)	physid_isset(apicid, bitmap)
#define check_apicid_present(bit)		physid_isset(bit, phys_cpu_present_map)

static inline int apic_id_registered(void)
{
	return physid_isset(GET_APIC_ID(apic_read(APIC_ID)), phys_cpu_present_map);
}

/*
 * Set up the logical destination ID.
 *
 * Intel recommends to set DFR, LDR and TPR before enabling
 * an APIC.  See e.g. "AP-388 82489DX User's Manual" (Intel
 * document number 292116).  So here it goes...
 */
static inline void init_apic_ldr(void)
{
	unsigned long val;

	apic_write_around(APIC_DFR, APIC_DFR_VALUE);
	val = apic_read(APIC_LDR) & ~APIC_LDR_MASK;
	val |= SET_APIC_LOGICAL_ID(1UL << smp_processor_id());
	apic_write_around(APIC_LDR, val);
}

static inline void summit_check(char *oem, char *productid) 
{
}

static inline void clustered_apic_check(void)
{
}

/* Mapping from cpu number to logical apicid */
static inline int cpu_to_logical_apicid(int cpu)
{
	return 1 << cpu;
}

static inline int cpu_present_to_apicid(int mps_cpu)
{
	return mps_cpu;
}

static inline physid_mask_t apicid_to_cpu_present(int apicid)
{
	return physid_mask_of_physid(apicid);
}

#define WAKE_SECONDARY_VIA_INIT

static inline void setup_portio_remap(void)
{
}

static inline void enable_apic_mode(void)
{
}

static inline int check_phys_apicid_present(int boot_cpu_physical_apicid)
{
	return physid_isset(boot_cpu_physical_apicid, phys_cpu_present_map);
}

static inline unsigned int cpu_mask_to_apicid(cpumask_const_t cpumask)
{
	return cpus_coerce_const(cpumask);
}
#endif /* __ASM_MACH_APIC_H */
