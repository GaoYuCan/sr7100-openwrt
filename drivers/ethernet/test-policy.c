/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "port-policy.h"

int main(void)
{
	unsigned int conf, running, source, flags, active, port, opening, checks=0;
	/* Explicit board expectation table, including holes and CPU/other ports. */
	const int physical[64] = {
		-1,0,1,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
	};
	for (conf=0; conf<8; conf++)
	for (running=0; running<8; running++)
	for (source=0; source<64; source++)
	for (flags=0; flags<16; flags++) {
		unsigned int w = source << 16 | (flags << 22) | 1500;
		int expected = physical[source];
		if (expected >= 0 && (!(conf & (1U << expected)) ||
				       !(running & (1U << expected))))
			expected = -1;
		assert(sr7100_ingress_smac(w, conf, running) == expected);
		checks++;
	}
	for (active=0; active<16; active++)
	for (port=0; port<8; port++)
	for (opening=0; opening<2; opening++) {
		unsigned int next=0xa5a5a5a5;
		int result=sr7100_port_transition(active, port, opening, &next);
		if (active>7 || port>=3 || !!(active & (1U << port)) == !!opening) {
			assert(result == SR7100_PORT_INVALID && next == 0xa5a5a5a5);
		} else {
			unsigned int expected = opening ? active | (1U << port) : active & ~(1U << port);
			assert(next == expected);
			assert(result == (opening ? (active ? SR7100_PORT_ADDITIONAL_OPEN : SR7100_ENGINE_FIRST_OPEN)
						 : (expected ? SR7100_PORT_SHARED_CLOSE : SR7100_ENGINE_LAST_CLOSE)));
		}
		checks++;
	}
	/* Concrete concurrent-port lifecycle: closing WAN cannot stop live LAN1. */
	active=0;
	assert(sr7100_port_transition(active,0,1,&active)==SR7100_ENGINE_FIRST_OPEN);
	assert(sr7100_port_transition(active,1,1,&active)==SR7100_PORT_ADDITIONAL_OPEN);
	assert(sr7100_port_transition(active,0,0,&active)==SR7100_PORT_SHARED_CLOSE);
	assert(active==2 && sr7100_ingress_smac(2U<<16,7,active)==1);
	assert(sr7100_ingress_smac(1U<<16,7,active)==-1);
	assert(sr7100_port_transition(active,1,0,&active)==SR7100_ENGINE_LAST_CLOSE);
	assert(active==0);
	assert(sr7100_egress_port(0)==1 && sr7100_egress_port(1)==2 && sr7100_egress_port(2)==3);
	assert(sr7100_egress_port(3)==-1 && sr7100_egress_port(~0U)==-1);
	printf("policy checks=%u; software only, no integrated driver or hardware isolation claim\n", checks);
	return 0;
}
