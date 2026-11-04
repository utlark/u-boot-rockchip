/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * Rockchip-format image header (non-OTA packaged kernel/boot images)
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __BOOT_ROCKCHIP_H_
#define __BOOT_ROCKCHIP_H_

struct rockchip_image {
	uint32_t tag;
	uint32_t size;
	int8_t image[1];
	uint32_t crc;
};

#define RK_BLK_SIZE			512
#define TAG_KERNEL			0x4C4E524B
#define TAG_BOOT			0x424E524B

#define PART_UBOOT			"uboot"
#define PART_MISC			"misc"
#define PART_RESOURCE			"resource"
#define PART_KERNEL			"kernel"
#define PART_BOOT			"boot"
#define PART_RECOVERY			"recovery"
#define PART_DTBO			"dtbo"
#define PART_ROOTFS			"rootfs"

#endif
