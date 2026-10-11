/* SPDX-License-Identifier: GPL-2.0-only */
/* Pure register policy recovered from SR7100 plat_132.ko.
 * This is NOT a MAC driver. A caller must still implement the vendor's
 * clock-gating, reset, filter, flow-control and SOPC sequencing.
 */
#ifndef SR7100_SMAC_POLICY_H
#define SR7100_SMAC_POLICY_H

#define SR7100_SMAC_FULL_DUPLEX (1U << 13)
#define SR7100_SMAC_SPEED_100   (1U << 14)
#define SR7100_SMAC_SPEED_LOW   (1U << 15)

static inline int sr7100_smac_link_config(unsigned int old, unsigned int speed,
					int full_duplex, unsigned int *next)
{
	unsigned int value;

	if (speed != 10 && speed != 100 && speed != 1000)
		return -1;
	value = old & ~SR7100_SMAC_FULL_DUPLEX;
	if (full_duplex)
		value |= SR7100_SMAC_FULL_DUPLEX;
	if (speed == 1000) {
		/* The original gigabit branch deliberately retains bit 14. */
		value &= ~SR7100_SMAC_SPEED_LOW;
	} else {
		value |= SR7100_SMAC_SPEED_LOW;
		value &= ~SR7100_SMAC_SPEED_100;
		if (speed == 100)
			value |= SR7100_SMAC_SPEED_100;
	}
	*next = value;
	return 0;
}

static inline int sr7100_smac_needs_reset(unsigned int old, unsigned int next)
{
	return !!((old ^ next) & SR7100_SMAC_FULL_DUPLEX);
}

#endif
