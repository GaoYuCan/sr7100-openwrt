// SPDX-License-Identifier: GPL-2.0-only
/* SR7100 single-port RAM laboratory: internal SMAC1, no XMAC/SerDes. */
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/phylink.h>
#include "zx279133.h"
#include "smac-policy.h"

#define SMAC_INDEX index
#define SMAC_BASE (0x40000 + SMAC_INDEX * 0x40000)
#define SOPC_READY (0x34294 + SMAC_INDEX * 4)
#define SOPC_SEND (0x342b0 + SMAC_INDEX * 4)

static void sr7100_smac_reset(struct zx279133_eth *eth, unsigned int index)
{
	u32 old = readl(eth->base + ZX279133_SYS_SOFT_RESET);

	writel(old & ~BIT(SMAC_INDEX), eth->base + ZX279133_SYS_SOFT_RESET);
	udelay(400);
	writel(old | BIT(SMAC_INDEX), eth->base + ZX279133_SYS_SOFT_RESET);
	readl(eth->base + ZX279133_SYS_SOFT_RESET);
}

/* Called after all software producers/IRQ/NAPI are stopped. Global PON reset
 * follows the vendor nppt_nppu_reset bit31 contract, with readback and settling
 * before any payload mappings/page-pool buffers are released. True quiescence
 * and reopen remain explicit hardware test requirements for this lab driver.
 */
void sr7100_datapath_reset(struct zx279133_eth *eth)
{
	u32 old = readl(eth->base + ZX279133_SYS_SOFT_RESET);

	writel(old & ~ZX279133_SYS_PON_RESET_N,
	       eth->base + ZX279133_SYS_SOFT_RESET);
	readl(eth->base + ZX279133_SYS_SOFT_RESET);
	udelay(400);
	writel(old | ZX279133_SYS_PON_RESET_N,
	       eth->base + ZX279133_SYS_SOFT_RESET);
	readl(eth->base + ZX279133_SYS_SOFT_RESET);
	udelay(400);
}

static void sr7100_flow(struct zx279133_eth *eth, unsigned int index, u32 cfg)
{
	u32 v = readl(eth->base + 0x343f0);

	if (cfg & SR7100_SMAC_FULL_DUPLEX)
		v &= ~BIT(SMAC_INDEX);
	else
		v |= BIT(SMAC_INDEX);
	writel(v, eth->base + 0x343f0);
}

static int sr7100_sopc_start(struct zx279133_eth *eth, unsigned int index)
{
	u32 value;
	int ret;

	/* Original sopc_send_enable polls 10 times with a 1000 us delay, then
	 * writes regardless. Lab version refuses to enable a non-ready port.
	 */
	ret = readl_poll_timeout(eth->base + SOPC_READY, value, value & 1,
				1000, 10000);
	if (ret)
		return dev_err_probe(eth->dev, ret, "Selected SMAC SOPC is not ready: %#x\n", value);
	usleep_range(1000, 1100);
	writel(1, eth->base + SOPC_SEND);
	return 0;
}

void sr7100_smac_enable(struct zx279133_eth *eth, unsigned int index, bool enable)
{
	void __iomem *mac = eth->base + SMAC_BASE;
	u32 cfg = readl(mac);

	cfg = enable ? cfg | 3 : cfg & ~3;
	writel(cfg, mac);
	sr7100_flow(eth, index, cfg);
}

static int sr7100_smac_init_one(struct zx279133_eth *eth, unsigned int index)
{
	void __iomem *mac = eth->base + SMAC_BASE;
	u32 v;

	/* nppt_smac_set_uni_mode(1, 0), then CPU133 nppt_smac_init sequence. */
	v = readl(eth->base + 4 * (SMAC_INDEX + 5));
	writel(v & 0xfc7fffff, eth->base + 4 * (SMAC_INDEX + 5));
	writel(0x00ba2203, mac);
	sr7100_smac_reset(eth, index);
	writel(0x00ba2200, mac);
	writel(0x3fff, mac + 4);
	writel(0x80000001, mac + 8);
	writel(readl(mac + 0xb00) | BIT(9), mac + 0xb00);
	writel(0x11200, mac + 0xe0);
	sr7100_flow(eth, index, 0x00ba2200);
	return sr7100_sopc_start(eth, index);
}

/* No PCS bypass exists for an internal SMAC; retained symbol for shared code. */
int zx279133_xpcs_set_bypass(struct zx279133_eth *eth, bool enabled) { return 0; }

static void sr7100_mac_config(struct phylink_config *config, unsigned int mode,
			     const struct phylink_link_state *state) { }

static void sr7100_link_down_index(struct zx279133_eth *eth, unsigned int index)
{
 mutex_lock(&eth->xmac_lock);
 if (eth->hardware_prepared) sr7100_smac_enable(eth, index, false);
 mutex_unlock(&eth->xmac_lock);
}

static void sr7100_link_up_index(struct zx279133_eth *eth, unsigned int index,
 int speed, int duplex)
{
	void __iomem *mac = eth->base + SMAC_BASE;
	u32 old, next, gate;
	int ret = 0;

	mutex_lock(&eth->xmac_lock);
	if (!eth->hardware_prepared)
		goto out;
	old = readl(mac);
	if (sr7100_smac_link_config(old, speed, duplex == DUPLEX_FULL, &next))
		goto out;
	writel(next & ~3, mac);
	sr7100_flow(eth, index, next);
	if (sr7100_smac_needs_reset(old, next)) {
		gate = readl(eth->base + 0xb8);
		writel(gate & ~BIT(4), eth->base + 0xb8);
		sr7100_smac_reset(eth, index);
		writel(next & ~3, mac);
		sr7100_flow(eth, index, next);
		writel(0x80000001, mac + 8);
		writel(0x11200, mac + 0xe0);
		writel(readl(mac + 0xd00) | BIT(1), mac + 0xd00);
		writel(readl(mac + 0xd30) | BIT(5), mac + 0xd30);
		ret = sr7100_sopc_start(eth, index);
		writel(gate, eth->base + 0xb8);
	}
	if (!ret) {
		sr7100_smac_enable(eth, index, true);
		dev_info(eth->dev, "SMAC%u enabled: %d Mbps %s duplex\n",
			 SMAC_INDEX, speed, duplex == DUPLEX_FULL ? "full" : "half");
	}
out:
	mutex_unlock(&eth->xmac_lock);
}

void zx279133_xmac_set_enabled(struct zx279133_eth *eth, bool enabled)
{
 for (unsigned int i=0; i<3; i++) sr7100_smac_enable(eth,i,enabled);
}
int sr7100_smac_init(struct zx279133_eth *eth)
{
 for (unsigned int i=0; i<3; i++) {
  int ret=sr7100_smac_init_one(eth,i);
  if (ret) return ret;
 }
 return 0;
}
static void sr7100_link_down(struct phylink_config *c,unsigned int mode,phy_interface_t interface)
{
 struct zx279133_eth *eth=container_of(c,struct zx279133_eth,phylink_config);
 sr7100_link_down_index(eth,1);
}
static void sr7100_link_up(struct phylink_config *c,struct phy_device *phy,unsigned int mode,
 phy_interface_t interface,int speed,int duplex,bool tx_pause,bool rx_pause)
{
 struct zx279133_eth *eth=container_of(c,struct zx279133_eth,phylink_config);
 sr7100_link_up_index(eth,1,speed,duplex);
}
static void sr7100_port_down(struct phylink_config *c,unsigned int mode,phy_interface_t interface)
{
 struct sr7100_port *p=container_of(c,struct sr7100_port,config);
 sr7100_link_down_index(p->eth,p->index);
}
static void sr7100_port_up(struct phylink_config *c,struct phy_device *phy,unsigned int mode,
 phy_interface_t interface,int speed,int duplex,bool tx_pause,bool rx_pause)
{
 struct sr7100_port *p=container_of(c,struct sr7100_port,config);
 sr7100_link_up_index(p->eth,p->index,speed,duplex);
}
const struct phylink_mac_ops zx279133_phylink_ops = {
 .mac_config=sr7100_mac_config,.mac_link_down=sr7100_link_down,.mac_link_up=sr7100_link_up,
};
const struct phylink_mac_ops sr7100_port_phylink_ops = {
 .mac_config=sr7100_mac_config,.mac_link_down=sr7100_port_down,.mac_link_up=sr7100_port_up,
};
