/* SPDX-License-Identifier: GPL-2.0-only */
/* All other commands fail closed before controller MMIO. Single-line SPI only. */
static inline bool sr7100_sfc_readonly_op(const struct spi_mem_op *op)
{
 if (op->cmd.nbytes != 1 || op->cmd.buswidth != 1 || op->cmd.dtr ||
     op->addr.dtr || op->dummy.dtr || op->data.dtr ||
     (op->addr.nbytes && op->addr.buswidth != 1) ||
     (op->dummy.nbytes && op->dummy.buswidth != 1) ||
     (op->data.nbytes && op->data.buswidth != 1) ||
     op->data.dir == SPI_MEM_DATA_OUT)
  return false;
 switch (op->cmd.opcode) {
 case 0xff: /* Volatile device reset, no array modification. */
  return !op->addr.nbytes && !op->dummy.nbytes && !op->data.nbytes &&
         op->data.dir == SPI_MEM_NO_DATA;
 case 0x9f: /* ID discovery: opcode, optional zero address or dummy byte. */
  return op->addr.nbytes <= 1 && !op->addr.val && op->dummy.nbytes <= 1 &&
         !(op->addr.nbytes && op->dummy.nbytes) &&
         op->data.dir == SPI_MEM_DATA_IN && op->data.nbytes > 0 && op->data.nbytes <= 8;
 case 0x0f: /* Read protection/config/status/ECC count registers only. */
  return op->addr.nbytes == 1 && !op->dummy.nbytes &&
         (op->addr.val == 0xa0 || op->addr.val == 0xb0 ||
          op->addr.val == 0xc0 || op->addr.val == 0x30) &&
         op->data.dir == SPI_MEM_DATA_IN && op->data.nbytes == 1;
 case 0x13: /* Array page -> internal volatile read cache, 65536 pages. */
  return op->addr.nbytes == 3 && op->addr.val < 65536 &&
         !op->dummy.nbytes && !op->data.nbytes && op->data.dir == SPI_MEM_NO_DATA;
 case 0x03: case 0x0b: /* Read cache: bounded page plus physical 96-byte OOB. */
  return op->addr.nbytes == 2 && op->addr.val < 2144 && op->dummy.nbytes == 1 &&
         op->data.dir == SPI_MEM_DATA_IN && op->data.nbytes > 0 &&
         op->data.nbytes <= 2144 - op->addr.val;
 default:
  return false;
 }
}
