// SPDX-License-Identifier: GPL-2.0-only
/* Preserve factory tuning; explicitly wake the PHY before negotiation.
 * Factory normal autoboot calls 0x388c4 and sets BMCR_PDOWN on all four PHYs.
 * The boot-menu/TFTP path bypasses that call. Do not soft-reset the PHY.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/phy.h>

static int sr7100_read_status(struct phy_device *phy)
{
	struct mii_bus *bus = phy->mdio.bus;
	int page, status = -EIO, restore;

	mutex_lock(&bus->mdio_lock);
	page = __mdiobus_read(bus, phy->mdio.addr, 0x1e);
	if (page < 0) {
		status = page;
		goto out;
	}
	status = __mdiobus_write(bus, phy->mdio.addr, 0x1e, 0);
	if (status < 0)
		goto restore_page;
	status = __mdiobus_read(bus, phy->mdio.addr, 0x1a);
	if (status >= 0) {
		udelay(100);
		status = __mdiobus_read(bus, phy->mdio.addr, 0x1a);
	}
restore_page:
	restore = __mdiobus_write(bus, phy->mdio.addr, 0x1e, page);
	if (restore < 0)
		status = restore;
out:
	mutex_unlock(&bus->mdio_lock);
	if (status < 0)
		return status;
	phy->link = !!(status & BIT(6));
	phy->speed = SPEED_UNKNOWN;
	phy->duplex = DUPLEX_UNKNOWN;
	phy->pause = phy->asym_pause = 0;
	if (phy->link) {
		static const int speeds[] = { SPEED_10, SPEED_100, SPEED_1000, SPEED_UNKNOWN };
		phy->speed = speeds[(status >> 8) & 3];
		phy->duplex = status & BIT(7) ? DUPLEX_FULL : DUPLEX_HALF;
	}
	return 0;
}

static struct phy_driver sr7100_gephy[] = {{
	.phy_id = 0x84b95032,
	.phy_id_mask = 0xffffffff,
	.name = "SR7100 integrated GEPHY",
	.features = PHY_GBIT_FEATURES,
	.config_init = genphy_resume,
	.config_aneg = genphy_config_aneg,
	.read_status = sr7100_read_status,
	.suspend = genphy_suspend,
	.resume = genphy_resume,
}};
module_phy_driver(sr7100_gephy);
static struct mdio_device_id ids[] = {{ 0x84b95032, 0xffffffff }, { }};
MODULE_DEVICE_TABLE(mdio, ids);
MODULE_DESCRIPTION("SR7100 integrated PHY status and power driver");
MODULE_LICENSE("GPL");
