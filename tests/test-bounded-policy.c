/* Execute the exact driver policy against every NAND page and opcode. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef uint64_t u64;
typedef uint8_t u8;
enum { SPI_MEM_NO_DATA, SPI_MEM_DATA_IN, SPI_MEM_DATA_OUT };
struct spi_mem_op {
 struct { unsigned nbytes, buswidth, dtr, opcode; } cmd;
 struct { unsigned nbytes, buswidth, dtr; u64 val; } addr;
 struct { unsigned nbytes, buswidth, dtr; } dummy;
 struct { unsigned nbytes, buswidth, dtr, dir; union { const void *out; void *in; } buf; } data;
};
#include "bounded-policy.h"
int main(void)
{
 unsigned checks=0;
 struct spi_mem_op o={.cmd={1,1,0,0x10},.addr={3,1,0,0},.data={.dir=SPI_MEM_NO_DATA}};
 for (unsigned row=0;row<65536;row++) {
  bool inside=row>=SR7100_OVERLAY_BEGIN/2048 && row<SR7100_OVERLAY_END/2048;
  o.addr.val=row;o.cmd.opcode=0x10;
  assert(sr7100_sfc_storage_op(&o)==inside);checks++;
  o.cmd.opcode=0xd8;
  assert(sr7100_sfc_storage_op(&o)==(inside && row%64==0));checks++;
 }
 o.addr.val=SR7100_OVERLAY_BEGIN/2048;
 for(unsigned opcode=0;opcode<256;opcode++) {
  o.cmd.opcode=opcode;
  assert(sr7100_sfc_storage_op(&o)==(opcode==0x13 || opcode==0x10 || opcode==0xd8));checks++;
 }
 u8 value;
 o.cmd.opcode=0x1f;o.addr.nbytes=1;o.data.dir=SPI_MEM_DATA_OUT;
 o.data.nbytes=1;o.data.buswidth=1;o.data.buf.out=&value;
 for(unsigned feature=0;feature<256;feature++) for(unsigned v=0;v<256;v++) {
  o.addr.val=feature;value=v;
  assert(sr7100_sfc_storage_op(&o)==((feature==0xa0 && v==0) || (feature==0xb0 && !(v&0xe0))));checks++;
 }
 o.cmd.opcode=0x02;o.addr.nbytes=2;
 for(unsigned column=0;column<=2144;column++) {
  o.addr.val=column;o.data.nbytes=1;
  assert(sr7100_sfc_storage_op(&o)==(column<2144));checks++;
  o.data.nbytes=2145-column;
  assert(!sr7100_sfc_storage_op(&o));checks++;
 }
 o.cmd.opcode=0x10;o.addr.nbytes=3;o.addr.val=SR7100_OVERLAY_BEGIN/2048;
 o.data.nbytes=0;o.data.dir=SPI_MEM_NO_DATA;
 for(unsigned width=2;width<=4;width++) {o.cmd.buswidth=width;assert(!sr7100_sfc_storage_op(&o));checks++;}
 o.cmd.buswidth=1;o.cmd.dtr=1;assert(!sr7100_sfc_storage_op(&o));checks++;
 printf("%u bounded NAND policy checks passed\n",checks);
}
