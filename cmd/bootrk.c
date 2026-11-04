// SPDX-License-Identifier: GPL-2.0+
/*
 * bootrk - boot a Rockchip-format kernel from the "kernel" partition
 *
 * Ported from the legacy Rockchip "bootrkp" command for the current
 * driver-model codebase.
 *
 * Image layout on the "kernel" partition (non-OTA packaged):
 *   [8-byte header: tag + size] [kernel image] [4-byte crc32]
 */

#include <common.h>
#include <blk.h>
#include <boot_rkimg.h>
#include <command.h>
#include <fdt_support.h>
#include <image.h>
#include <malloc.h>
#include <memblk.h>
#include <part.h>
#include <sysmem.h>
#include <asm/global_data.h>
#include <asm/arch-rockchip/common.h>
#include <linux/libfdt.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * read_rockchip_image() - Read a rockchip-format image from a partition
 *
 * @dev_desc:	block device
 * @part:		destination partition info
 * @dst:		destination address (8-byte header is stripped)
 *
 * Return: payload size on success, -ve on error
 */
static int read_rockchip_image(struct blk_desc *dev_desc,
			       struct disk_partition *part, void *dst)
{
	struct rockchip_image *img = dst;
	int cnt, ret;

	ret = blk_dread(dev_desc, part->start, 1, img);
	if (ret != 1)
		return -EIO;

	if (img->tag != TAG_KERNEL) {
		printf("Invalid %s image tag(0x%x)\n", part->name, img->tag);
		return -EINVAL;
	}

	/* total size = image size + 8 bytes header + 4 bytes crc32 */
	cnt = DIV_ROUND_UP(img->size + 8 + 4, RK_BLK_SIZE);
	if (!sysmem_alloc_base_by_name(part->name, (phys_addr_t)dst,
				       cnt * dev_desc->blksz))
		return -ENXIO;

	ret = blk_dread(dev_desc, part->start, cnt, dst);
	if (ret != cnt)
		return -EIO;

	memcpy(dst, dst + 8, img->size);

	return img->size;
}

int do_bootrk(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct blk_desc *dev_desc;
	struct disk_partition kernel_part, boot_part;
	ulong kernel_addr_r, ramdisk_addr_r, fdt_addr_r;
	int kernel_size, ramdisk_size;
	char cmdbuf[80];
	int ret;

	dev_desc = plat_bootdev();
	if (!dev_desc) {
		printf("No boot device!\n");
		return CMD_RET_FAILURE;
	}

	kernel_addr_r = env_get_ulong("kernel_addr_r", 16, 0x02000000);
	ramdisk_addr_r = env_get_ulong("ramdisk_addr_r", 16, 0);
	fdt_addr_r = env_get_ulong("fdt_addr_r", 16, 0);

	if (part_get_info_by_name(dev_desc, PART_KERNEL, &kernel_part) < 0) {
		printf("Could not find kernel partition\n");
		return CMD_RET_FAILURE;
	}

	kernel_size = read_rockchip_image(dev_desc, &kernel_part,
					  (void *)kernel_addr_r);
	if (kernel_size < 0) {
		printf("Failed to read kernel image, ret=%d\n", kernel_size);
		return CMD_RET_FAILURE;
	}

	/* ramdisk is optional (boot partition, same rockchip format) */
	ramdisk_size = 0;
	if (!part_get_info_by_name(dev_desc, PART_BOOT, &boot_part)) {
		ramdisk_size = read_rockchip_image(dev_desc, &boot_part,
						   (void *)ramdisk_addr_r);
		if (ramdisk_size < 0)
			ramdisk_size = 0;
	}

	/*
	 * DTB: prefer rk-kernel.dtb from the resource partition, fall back
	 * to the current in-memory FDT if it is not available.
	 */
	const void *fdt = gd->fdt_blob;

	if (fdt_addr_r && gd->fdt_blob != (void *)fdt_addr_r) {
		if (rockchip_read_dtb_file((void *)fdt_addr_r)) {
			int fdt_size = fdt_totalsize(gd->fdt_blob);

			printf("Resource DTB unavailable, using current FDT\n");
			if (!sysmem_alloc_base(MEM_FDT, (phys_addr_t)fdt_addr_r,
					       ALIGN(fdt_size, RK_BLK_SIZE) +
					       CONFIG_SYS_FDT_PAD))
				return CMD_RET_FAILURE;
			memcpy((void *)fdt_addr_r, gd->fdt_blob, fdt_size);
		} else {
			fdt = (void *)fdt_addr_r;
		}
	}

	/*
	 * Kernel command line: take the "bootargs" property from the DTB
	 * (the legacy Rockchip U-Boot used exactly this); fall back to a
	 * minimal console+root line if the DTB has none.
	 */
	{
		const char *ba = fdt_getprop(fdt, 0, "bootargs", NULL);

		if (ba && *ba) {
			env_set("bootargs", ba);
		} else {
			char bootargs[128];

			snprintf(bootargs, sizeof(bootargs),
				 "console=ttyFIQ0 root=/dev/mmcblk%dp9 rootwait "
				 "rootfstype=ext4",
				 dev_desc->devnum);
			env_set("bootargs", bootargs);
		}
		printf("bootargs: %s\n", env_get("bootargs"));
	}

	printf("kernel   @ 0x%08lx (0x%08x)\n", kernel_addr_r, kernel_size);
	printf("ramdisk  @ 0x%08lx (0x%08x)\n", ramdisk_addr_r, ramdisk_size);
	printf("fdt      @ 0x%08lx\n", fdt_addr_r);

	snprintf(cmdbuf, sizeof(cmdbuf),
		 "booti 0x%lx 0x%lx:0x%x 0x%lx",
		 kernel_addr_r, ramdisk_addr_r, ramdisk_size, fdt_addr_r);
	ret = run_command(cmdbuf, 0);
	if (ret)
		printf("booti failed (err=%d)\n", ret);

	return ret;
}

U_BOOT_CMD(
	bootrk, 1, 1, do_bootrk,
	"boot Rockchip-format kernel from the 'kernel' partition",
	""
);
