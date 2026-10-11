/* SPDX-License-Identifier: GPL-2.0-only */
/* Only the independent Plugin array range can be programmed or erased.
 * Even the parent MTD device cannot modify a bootloader, calibration or rootfs.
 * Feature-register writes exclude all OTP/lock modes. Single-line SPI only.
 */
#include "readonly-policy.h"
#define SR7100_OVERLAY_BEGIN 0x036c0000ULL
#define SR7100_OVERLAY_END   0x08000000ULL
static inline bool sr7100_sfc_storage_op(const struct spi_mem_op *op)
{
 u64 row;
 if (sr7100_sfc_readonly_op(op))
  return true;
 if (op->cmd.nbytes!=1 || op->cmd.buswidth!=1 || op->cmd.dtr ||
     op->addr.dtr || op->dummy.dtr || op->data.dtr || op->dummy.nbytes ||
     (op->addr.nbytes && op->addr.buswidth!=1) ||
     (op->data.nbytes && op->data.buswidth!=1))
  return false;
 switch (op->cmd.opcode) {
 case 0x06: case 0x04: /* Volatile WREN/WRDI, no array modification alone. */
  return !op->addr.nbytes && !op->data.nbytes && op->data.dir==SPI_MEM_NO_DATA;
 case 0x1f: /* Volatile feature updates; never enter or lock OTP. */
  if (op->addr.nbytes!=1 || op->data.dir!=SPI_MEM_DATA_OUT ||
      op->data.nbytes!=1 || !op->data.buf.out)
   return false;
  if (op->addr.val==0xa0)
   return *(const u8 *)op->data.buf.out==0;
  return op->addr.val==0xb0 && !(*(const u8 *)op->data.buf.out & 0xe0);
 case 0x02: case 0x84: /* Volatile program-cache load/update. */
  return op->addr.nbytes==2 && op->addr.val<2144 &&
         op->data.dir==SPI_MEM_DATA_OUT && op->data.nbytes>0 &&
         op->data.nbytes<=2144-op->addr.val && op->data.buf.out;
 case 0x10: case 0xd8: /* Array program execute/block erase. */
  row=op->addr.val;
  return op->addr.nbytes==3 && !op->data.nbytes &&
         op->data.dir==SPI_MEM_NO_DATA &&
         row>=SR7100_OVERLAY_BEGIN/2048 && row<SR7100_OVERLAY_END/2048 &&
         (op->cmd.opcode!=0xd8 || row%64==0);
 default:
  return false;
 }
}
