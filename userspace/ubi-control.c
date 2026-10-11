/* SPDX-License-Identifier: GPL-2.0-only
 * Plugin only. Normal boot never creates volumes or erases a filesystem.
 * Provisioning requires a separately erased partition and an explicit mode.
 */
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <mtd/ubi-user.h>
#include <mtd/mtd-user.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define PLUGIN_BYTES 0x4940000UL
static void die(const char *s) { perror(s); exit(1); }
static unsigned long number(const char *path) {
 unsigned long n; FILE *f=fopen(path,"r");
 if(!f) die(path);
 if(fscanf(f,"%lu",&n)!=1) {errno=EINVAL;die(path);} fclose(f); return n;
}
static unsigned long attribute(int mtd,const char *name) {
 char p[128];snprintf(p,sizeof p,"/sys/class/mtd/mtd%d/%s",mtd,name);return number(p);
}
static uint32_t be32(const unsigned char *p) {
 return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
}
static uint32_t ubi_crc(const unsigned char *p,size_t n) {
 uint32_t c=~0U;
 while(n--) {c^=*p++;for(int k=0;k<8;k++) c=(c>>1)^(0xedb88320U & (0U-(c&1)));}
 return c;
}
static int find_plugin(void) {
 int found=-1;
 for(int i=0;i<32;i++) {
  char path[128],name[80];snprintf(path,sizeof path,"/sys/class/mtd/mtd%d/name",i);
  FILE *f=fopen(path,"r");if(!f) continue;
  int match=fgets(name,sizeof name,f) && !strcmp(name,"Plugin\n");fclose(f);
  if(match) {if(found>=0){errno=EINVAL;die("duplicate Plugin");}found=i;}
 }
 if(found<0 || attribute(found,"size")!=PLUGIN_BYTES || attribute(found,"erasesize")!=0x20000 || attribute(found,"writesize")!=2048) {
  errno=EINVAL;die("unexpected Plugin geometry");
 }
 return found;
}
/* Bad blocks are skipped using the MTD API; short reads are fatal. */
static void inspect(int mtd,int *blank,int *valid_ec) {
 char path[64];unsigned char buf[2048];*blank=1;*valid_ec=0;
 snprintf(path,sizeof path,"/dev/mtd%d",mtd);int fd=open(path,O_RDONLY);
 if(fd<0) die(path);
 for(loff_t block=0;block<(loff_t)PLUGIN_BYTES;block+=0x20000) {
  int bad=ioctl(fd,MEMGETBADBLOCK,&block);if(bad<0)die("bad-block query");if(bad)continue;
  for(unsigned page=0;page<64;page++) {
   if(pread(fd,buf,sizeof buf,block+page*2048)!=(ssize_t)sizeof buf){errno=EIO;die("Plugin read");}
   for(unsigned j=0;j<sizeof buf;j++) if(buf[j]!=255)*blank=0;
   if(!page && be32(buf)==0x55424923 && buf[4]==1 &&
      be32(buf+16)==2048 && be32(buf+20)==4096 &&
      be32(buf+60)==ubi_crc(buf,60)) (*valid_ec)++;
  }
 }
 close(fd);
}
int main(int argc,char **argv) {
 int initialize=argc==2 && !strcmp(argv[1],"initialize-erased");
 if(argc!=2 || (!initialize && strcmp(argv[1],"attach"))) {
  fputs("usage: sr7100-plugin-ubi attach | initialize-erased\n",stderr);return 2;
 }
 int mtd=find_plugin(),blank,valid;
 /* Reserve UBI 0 exclusively for this overlay, never detach other devices. */
 if(access("/sys/class/ubi/ubi0",F_OK)==0){errno=EBUSY;die("UBI 0 already attached");}
 inspect(mtd,&blank,&valid);
 if(initialize ? !blank : (blank || !valid)) {errno=EINVAL;die("Plugin is not in the expected provisioning/UBI state");}
 struct ubi_attach_req req={.ubi_num=0,.mtd_num=mtd,.max_beb_per1024=20,.disable_fm=1};
 int fd=open("/dev/ubi_ctrl",O_RDWR);if(fd<0)die("ubi_ctrl");
 if(ioctl(fd,UBI_IOCATT,&req)<0)die("UBI attach");
 close(fd);
 if(initialize) {
  if(number("/sys/class/ubi/ubi0/volumes_count")){errno=EBUSY;die("existing volumes");}
  unsigned long n=number("/sys/class/ubi/ubi0/avail_eraseblocks");
  unsigned long sz=number("/sys/class/ubi/ubi0/eraseblock_size");
  if(n<17 || n>586 || sz!=126976){errno=EINVAL;die("UBI geometry");}
  struct ubi_mkvol_req v={.vol_id=0,.alignment=1,.bytes=(int64_t)n*sz,.vol_type=UBI_DYNAMIC_VOLUME,.name_len=11};
  strcpy(v.name,"rootfs_data");fd=open("/dev/ubi0",O_RDWR);
  if(fd<0 || ioctl(fd,UBI_IOCMKVOL,&v)<0)die("create rootfs_data");
  close(fd);
 }
 puts(initialize?"PLUGIN_INITIALIZED":"PLUGIN_ATTACHED");return 0;
}
