/* SPDX-License-Identifier: GPL-2.0-only */
/* Included by netdev.c to reuse its single DMA engine primitives. */
#include "port-policy.h"

static struct sr7100_port *sr7100_netdev_port(struct net_device *ndev)
{
 if (ndev->netdev_ops == &sr7100_main_netdev_ops) {
  struct zx279133_eth *eth=netdev_priv(ndev);
  return &eth->ports[1];
 }
 return *(struct sr7100_port **)netdev_priv(ndev);
}

int sr7100_ndev_index(struct zx279133_eth *eth, struct net_device *ndev)
{
 for (int i=0; i<3; i++) if (eth->ports[i].ndev==ndev) return i;
 WARN_ON_ONCE(1);
 return 1;
}

static int sr7100_shared_open(struct net_device *ndev)
{
 struct sr7100_port *p=sr7100_netdev_port(ndev);
 struct zx279133_eth *eth=p->eth;
 unsigned int next;
 int transition,ret;
 bool first;
 mutex_lock(&eth->datapath_lock);
 if (eth->shared_faulted) { ret=-EIO; goto out; }
 transition=sr7100_port_transition(eth->datapath_users,p->index,1,&next);
 if (transition<0) { ret=-EBUSY; goto out; }
 first=transition==SR7100_ENGINE_FIRST_OPEN;
 if (first) {
  ret=zx279133_hardware_prepare(eth);
  if (ret) goto out;
  ret=zx279133_shared_idm_prepare(eth);
  if (ret) goto unprepare;
 }
 ret=phylink_of_phy_connect(p->phylink,p->np,0);
 if (ret) goto abort;
 WRITE_ONCE(eth->datapath_users,next);
 phylink_start(p->phylink);
 if (first) zx279133_shared_rx_start(eth);
 netif_tx_start_all_queues(ndev);
 mutex_unlock(&eth->datapath_lock);
 return 0;
abort:
 if (first) zx279133_shared_idm_abort(eth);
unprepare:
 if (first) zx279133_hardware_unprepare(eth);
out:
 mutex_unlock(&eth->datapath_lock);
 return ret;
}

static int sr7100_shared_stop(struct net_device *ndev)
{
 struct sr7100_port *p=sr7100_netdev_port(ndev);
 struct zx279133_eth *eth=p->eth;
 unsigned int next,active;
 bool last,drained;
 mutex_lock(&eth->datapath_lock);
 active=eth->datapath_users;
 if (!(active & BIT(p->index))) goto out;
 if (sr7100_port_transition(active,p->index,0,&next)<0) goto out;
 last=!next;
 netif_tx_disable(ndev);
 if (last) zx279133_shared_rx_stop(eth);
 drained=zx279133_shared_tx_pause(eth);
 /* A drain timeout must not leave DMA referencing a departing netdev.
  * Stop all hardware and retain an explicit fault until reload/reboot.
  */
 if (!drained && !last) {
  eth->shared_faulted=true;
  zx279133_shared_rx_stop(eth);
  for (int i=0;i<3;i++) if (active & BIT(i)) {
   phylink_stop(eth->ports[i].phylink);
   phylink_disconnect_phy(eth->ports[i].phylink);
   netif_carrier_off(eth->ports[i].ndev);
  }
  WRITE_ONCE(eth->datapath_users,0);
  zx279133_shared_idm_release(eth,ndev,false);
  netdev_err(ndev,"shared DMA drain failed; all ports stopped until reload\n");
  goto out;
 }
 phylink_stop(p->phylink);
 phylink_disconnect_phy(p->phylink);
 WRITE_ONCE(eth->datapath_users,next);
 /* Exclude in-flight RX delivery before netdev unregister can free stats. */
 synchronize_net();
 if (last) zx279133_shared_idm_release(eth,ndev,drained);
 else zx279133_shared_tx_resume(eth);
out:
 mutex_unlock(&eth->datapath_lock);
 return 0;
}

static netdev_tx_t sr7100_shared_xmit(struct sk_buff *skb,struct net_device *ndev)
{
 struct sr7100_port *p=sr7100_netdev_port(ndev);
 if (unlikely(READ_ONCE(p->eth->tx_stopping) ||
              !(READ_ONCE(p->eth->datapath_users)&BIT(p->index)))) {
  netif_stop_subqueue(ndev,skb_get_queue_mapping(skb));
  return NETDEV_TX_BUSY;
 }
 return zx279133_start_xmit_common(skb,p->eth->ndev,ndev);
}
static void sr7100_shared_stats(struct net_device *ndev,struct rtnl_link_stats64 *stats)
{
 struct sr7100_port *p=sr7100_netdev_port(ndev);
 zx279133_fill_stats64(ndev,zx279133_netdev_stats(p->eth,ndev),stats);
}
static void sr7100_shared_timeout(struct net_device *ndev,unsigned int queue)
{
 struct sr7100_port *p=sr7100_netdev_port(ndev);
 zx279133_tx_timeout_common(p->eth,ndev,queue);
}
static int sr7100_shared_ioctl(struct net_device *ndev,struct ifreq *ifr,int cmd)
{
 return phylink_mii_ioctl(sr7100_netdev_port(ndev)->phylink,ifr,cmd);
}
static int sr7100_shared_ksettings_get(struct net_device *ndev,struct ethtool_link_ksettings *cmd)
{
 return phylink_ethtool_ksettings_get(sr7100_netdev_port(ndev)->phylink,cmd);
}
static int sr7100_shared_ksettings_set(struct net_device *ndev,const struct ethtool_link_ksettings *cmd)
{
 return phylink_ethtool_ksettings_set(sr7100_netdev_port(ndev)->phylink,cmd);
}
static const struct ethtool_ops sr7100_shared_ethtool_ops={
 .get_drvinfo=zx279133_get_drvinfo,.get_link=ethtool_op_get_link,
 .get_link_ksettings=sr7100_shared_ksettings_get,.set_link_ksettings=sr7100_shared_ksettings_set,
};
#define SR7100_PORT_NDO \
 .ndo_open=sr7100_shared_open,.ndo_stop=sr7100_shared_stop, \
 .ndo_start_xmit=sr7100_shared_xmit,.ndo_get_stats64=sr7100_shared_stats, \
 .ndo_tx_timeout=sr7100_shared_timeout,.ndo_eth_ioctl=sr7100_shared_ioctl, \
 .ndo_validate_addr=eth_validate_addr,.ndo_set_mac_address=eth_mac_addr
const struct net_device_ops sr7100_main_netdev_ops={SR7100_PORT_NDO};
static const struct net_device_ops sr7100_extra_netdev_ops={SR7100_PORT_NDO};

static void sr7100_port_destroy(void *data) { phylink_destroy(data); }
static void sr7100_node_put(void *data) { of_node_put(data); }
int sr7100_prepare_ports(struct zx279133_eth *eth)
{
 struct device *dev=eth->dev;
 static const char * const names[]={"wan","lan1","lan2"};
 eth->ndev->ethtool_ops=&sr7100_shared_ethtool_ops;
 for (unsigned int i=0;i<3;i++) {
  struct sr7100_port *p=&eth->ports[i];
  struct net_device *ndev;
  u8 address[ETH_ALEN];
  int ret;
  if (i==1) continue;
  p->eth=eth;p->index=i;
  p->np=of_get_child_by_name(dev->of_node,i==0?"wan-port":"lan2-port");
  if (!p->np) return -ENODEV;
  ret=devm_add_action_or_reset(dev,sr7100_node_put,p->np);
  if (ret) return ret;
  ndev=devm_alloc_etherdev_mqs(dev,sizeof(struct sr7100_port *),ZX279133_CPU_TX_QUEUES,ZX279133_CPU_RX_QUEUES);
  if (!ndev) return -ENOMEM;
  p->ndev=ndev;
  *(struct sr7100_port **)netdev_priv(ndev)=p;
  SET_NETDEV_DEV(ndev,dev);
  strscpy(ndev->name,names[i],IFNAMSIZ);
  ndev->netdev_ops=&sr7100_extra_netdev_ops;
  ndev->ethtool_ops=&sr7100_shared_ethtool_ops;
  ndev->watchdog_timeo=HZ;
  ndev->min_mtu=ETH_MIN_MTU;ndev->max_mtu=ETH_DATA_LEN;
  ndev->tstats=devm_netdev_alloc_pcpu_stats(dev,struct pcpu_sw_netstats);
  if (!ndev->tstats) return -ENOMEM;
  zx279133_netdev_stats_init(&p->stats);
  ether_addr_copy(address,eth->ndev->dev_addr);
  address[0]=(address[0]|2)&~1;
  address[ETH_ALEN-1]^=i^1;
  eth_hw_addr_set(ndev,address);
  p->config.dev=&ndev->dev;p->config.type=PHYLINK_NETDEV;
  p->config.mac_capabilities=MAC_1000FD|MAC_100|MAC_10;
  __set_bit(PHY_INTERFACE_MODE_INTERNAL,p->config.supported_interfaces);
  p->phylink=phylink_create(&p->config,of_fwnode_handle(p->np),PHY_INTERFACE_MODE_INTERNAL,&sr7100_port_phylink_ops);
  if (IS_ERR(p->phylink)) return PTR_ERR(p->phylink);
  ret=devm_add_action_or_reset(dev,sr7100_port_destroy,p->phylink);
  if (ret) return ret;
 }
 return 0;
}
int sr7100_register_ports(struct zx279133_eth *eth)
{
 for (int i=0;i<3;i++) if(i!=1) {
  int ret=devm_register_netdev(eth->dev,eth->ports[i].ndev);
  if(ret) return ret;
 }
 return 0;
}
