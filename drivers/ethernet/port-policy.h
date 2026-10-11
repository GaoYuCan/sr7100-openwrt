/* SPDX-License-Identifier: GPL-2.0-only */
/* SR7100 physical WAN/LAN1/LAN2 mapping, confirmed in RAM experiment 23.
 * Pure policy only; caller still owns descriptor validation and lifecycle
 * serialization. Integrated into the tested SR7100 shared three-port driver.
 */
#ifndef SR7100_PORT_POLICY_H
#define SR7100_PORT_POLICY_H

#define SR7100_BOARD_PORT_MASK 7U

/* Reject unsupported, unconfigured and administratively stopped ingress.
 * No fallback to a LAN device: misclassified frames must not cross zones.
 * Packet length, jumbo/error flags and DMA ownership are checked by caller.
 */
static inline int sr7100_ingress_smac(unsigned int word1,
				    unsigned int configured,
				    unsigned int running)
{
	unsigned int source = (word1 >> 16) & 0x3f;
	unsigned int index;

	if (source < 1 || source > 3)
		return -1;
	index = source - 1;
	if (!(configured & running & SR7100_BOARD_PORT_MASK & (1U << index)))
		return -1;
	return index;
}

static inline int sr7100_egress_port(unsigned int smac)
{
	return smac < 3 ? (int)smac + 1 : -1;
}

enum sr7100_port_transition {
	SR7100_PORT_INVALID = -1,
	SR7100_ENGINE_FIRST_OPEN = 1,
	SR7100_PORT_ADDITIONAL_OPEN,
	SR7100_PORT_SHARED_CLOSE,
	SR7100_ENGINE_LAST_CLOSE,
};

/* Caller holds RTNL/datapath lifecycle lock. Policy does not mutate hardware.
 * The mask commits only after the corresponding hardware operation succeeds.
 * Duplicate open/close cannot double-start IRQ/NAPI or free a shared DMA ring.
 */
static inline int sr7100_port_transition(unsigned int active,
				       unsigned int smac, int opening,
				       unsigned int *next)
{
	unsigned int bit;

	if ((active & ~SR7100_BOARD_PORT_MASK) || smac >= 3)
		return SR7100_PORT_INVALID;
	bit = 1U << smac;
	if (opening) {
		if (active & bit)
			return SR7100_PORT_INVALID;
		*next = active | bit;
		return active ? SR7100_PORT_ADDITIONAL_OPEN : SR7100_ENGINE_FIRST_OPEN;
	}
	if (!(active & bit))
		return SR7100_PORT_INVALID;
	*next = active & ~bit;
	return *next ? SR7100_PORT_SHARED_CLOSE : SR7100_ENGINE_LAST_CLOSE;
}

#endif
