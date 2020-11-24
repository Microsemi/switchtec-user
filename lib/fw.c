/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2017, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * @file
 * @brief Switchtec core library functions for firmware operations
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/utils.h"
#include "switchtec/mfg.h"

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * @defgroup Firmware Firmware Management
 * @brief Retrieve firmware information and update or retrieve images
 *
 * switchtec_fw_write_fd() may be used to update a Switchtec firmware
 * image. switchtec_fw_read_fd() can retrieve a firmware image into a
 * local file. switchtec_fw_img_info() and switchtec_fw_cfg_info() may
 * be used to query information about the currently programmed images.
 * @{
 */

struct switchtec_fw_footer_gen3 {
	char magic[4];
	uint32_t image_len;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd;
	uint32_t header_crc;
	uint32_t image_crc;
};

enum switchtec_fw_part_type_gen4 {
	SWITCHTEC_FW_IMG_TYPE_MAP_GEN4 = 0x0,
	SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN4 = 0x1,
	SWITCHTEC_FW_IMG_TYPE_BL2_GEN4 = 0x2,
	SWITCHTEC_FW_IMG_TYPE_CFG_GEN4 = 0x3,
	SWITCHTEC_FW_IMG_TYPE_IMG_GEN4 = 0x4,
	SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN4 = 0x5,
	SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN4 = 0xFE,
	SWITCHTEC_FW_IMG_TYPE_UNKNOWN_GEN4,
};

struct switchtec_fw_image_header_gen3 {
	char magic[4];
	uint32_t image_len;
	uint32_t type;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd[9];
	uint32_t header_crc;
	uint32_t image_crc;
};

static int switchtec_fw_dlstatus(struct switchtec_dev *dev,
				 enum switchtec_fw_dlstatus *status,
				 enum mrpc_bg_status *bgstatus)
{
	uint32_t cmd = MRPC_FWDNLD;
	uint32_t subcmd = MRPC_FWDNLD_GET_STATUS;
	struct {
		uint8_t dlstatus;
		uint8_t bgstatus;
		uint16_t reserved;
	} result;
	int ret;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW)
		cmd = MRPC_FW_TX;

	ret = switchtec_cmd(dev, cmd, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	if (ret)
		return ret;

	if (status != NULL)
		*status = result.dlstatus;

	if (bgstatus != NULL)
		*bgstatus = result.bgstatus;

	return 0;
}

static int switchtec_fw_wait(struct switchtec_dev *dev,
			     enum switchtec_fw_dlstatus *status)
{
	enum mrpc_bg_status bgstatus;
	int ret;

	do {
		// Delay slightly to avoid interrupting the firmware too much
		usleep(5000);

		ret = switchtec_fw_dlstatus(dev, status, &bgstatus);
		if (ret < 0)
			return ret;
		if (*status != SWITCHTEC_DLSTAT_INPROGRESS &&
		    *status != SWITCHTEC_DLSTAT_COMPLETES &&
		    *status != SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT &&
		    *status != SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT)
			return *status;
		if (bgstatus == MRPC_BG_STAT_ERROR)
			return SWITCHTEC_DLSTAT_HARDWARE_ERR;

	} while (bgstatus == MRPC_BG_STAT_INPROGRESS);

	return 0;
}

/**
 * @brief Toggle the active firmware partition for the main or configuration
 *	images.
 * @param[in] dev        Switchtec device handle
 * @param[in] toggle_bl2 Set to 1 to toggle the BL2 FW image
 * @param[in] toggle_key Set to 1 to toggle the key manifest FW image
 * @param[in] toggle_fw  Set to 1 to toggle the main FW image
 * @param[in] toggle_cfg Set to 1 to toggle the config FW image
 * @return 0 on success, error code on failure
 */
int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_bl2, int toggle_key,
					 int toggle_fw, int toggle_cfg)
{
	uint32_t cmd_id;
	struct {
		uint8_t subcmd;
		uint8_t toggle_fw;
		uint8_t toggle_cfg;
		uint8_t toggle_bl2;
		uint8_t toggle_key;
	} cmd;

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		cmd_id = MRPC_FW_TX;
		cmd.subcmd = MRPC_FW_TX_TOGGLE;
	} else {
		cmd_id = MRPC_FWDNLD;
		cmd.subcmd = MRPC_FWDNLD_TOGGLE;
	}

	cmd.toggle_bl2 = !!toggle_bl2;
	cmd.toggle_key = !!toggle_key;
	cmd.toggle_fw = !!toggle_fw;
	cmd.toggle_cfg = !!toggle_cfg;

	return switchtec_cmd(dev, cmd_id, &cmd, sizeof(cmd),
			     NULL, 0);
}

static enum switchtec_fw_part_type_gen4
switchtec_fw_type_gen4(enum switchtec_fw_type type)
{
	switch (type) {
	case SWITCHTEC_FW_TYPE_MAP:
		return SWITCHTEC_FW_IMG_TYPE_MAP_GEN4;
	case SWITCHTEC_FW_TYPE_IMG:
		return SWITCHTEC_FW_IMG_TYPE_IMG_GEN4;
	case SWITCHTEC_FW_TYPE_CFG:
		return SWITCHTEC_FW_IMG_TYPE_CFG_GEN4;
	case SWITCHTEC_FW_TYPE_NVLOG:
		return SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN4;
	case SWITCHTEC_FW_TYPE_SEEPROM:
		return SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN4;
	case SWITCHTEC_FW_TYPE_KEY:
		return SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN4;
	case SWITCHTEC_FW_TYPE_BL2:
		return SWITCHTEC_FW_IMG_TYPE_BL2_GEN4;
	default:
		return SWITCHTEC_FW_IMG_TYPE_UNKNOWN_GEN4;
	};
}

/**
 * @brief Set or clear the redundancy flag of a partition type
 * @param[in] dev		Switchtec device handle
 * @param[in] redund		Whether to set or clear the redundancy flag
 * @param[in] type		Switchtec fw partition type
 * @return 0 on success, error code on failure
 *
 * This function does not support Gen3 switch.
 */
int switchtec_fw_setup_redundancy(struct switchtec_dev *dev,
				  enum switchtec_fw_redundancy redund,
				  enum switchtec_fw_type type)
{
	int ret;

	struct set_fw_redundancy{
		uint8_t sub_cmd;
		uint8_t part_type;
		uint8_t flag;
		uint8_t rsvd;
	} cmd = {
		.sub_cmd = MRPC_FWDNLD_SET_REDUNDANCY,
		.part_type = switchtec_fw_type_gen4(type),
		.flag = redund,
	};

	if (switchtec_is_gen3(dev)) {
		errno = ENOTSUP;
		return -1;
	}

	ret = switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd), NULL, 0);

	return ret;
}

struct cmd_fwdl {
	struct cmd_fwdl_hdr {
		uint8_t subcmd;
		uint8_t dont_activate;
		uint8_t reserved[2];
		uint32_t offset;
		uint32_t img_length;
		uint32_t blk_length;
	} hdr;
	uint8_t data[MRPC_MAX_DATA_LEN - sizeof(struct cmd_fwdl_hdr)];
};

/**
 * @brief Write a firmware file to the switchtec device
 * @param[in] dev		Switchtec device handle
 * @param[in] img_fd		File descriptor for the image file to write
 * @param[in] force		If 1, ignore if another download command is
 *			        already in progress.
 * @param[in] dont_activate	If 1, the new image will not be activated
 * @param[in] progress_callback If not NULL, this function will be called to
 * 	indicate the progress.
 * @return 0 on success, error code on failure
 */
int switchtec_fw_write_fd(struct switchtec_dev *dev, int img_fd,
			  int dont_activate, int force,
			  void (*progress_callback)(int cur, int tot))
{
	enum switchtec_fw_dlstatus status;
	enum mrpc_bg_status bgstatus;
	ssize_t image_size, offset = 0;
	int ret;
	struct cmd_fwdl cmd = {};
	uint32_t cmd_id = MRPC_FWDNLD;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW)
		cmd_id = MRPC_FW_TX;

	image_size = lseek(img_fd, 0, SEEK_END);
	if (image_size < 0)
		return -errno;
	lseek(img_fd, 0, SEEK_SET);

	switchtec_fw_dlstatus(dev, &status, &bgstatus);

	if (!force && status == SWITCHTEC_DLSTAT_INPROGRESS) {
		errno = EBUSY;
		return -EBUSY;
	}

	if (bgstatus == MRPC_BG_STAT_INPROGRESS) {
		errno = EBUSY;
		return -EBUSY;
	}

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2)
		cmd.hdr.subcmd = MRPC_FW_TX_FLASH;
	else
		cmd.hdr.subcmd = MRPC_FWDNLD_DOWNLOAD;

	cmd.hdr.dont_activate = !!dont_activate;
	cmd.hdr.img_length = htole32(image_size);

	while (offset < image_size) {
		ssize_t blklen = read(img_fd, &cmd.data,
				      sizeof(cmd.data));

		if (blklen == -EAGAIN || blklen == -EWOULDBLOCK)
			continue;

		if (blklen < 0)
			return -errno;

		if (blklen == 0)
			break;

		cmd.hdr.offset = htole32(offset);
		cmd.hdr.blk_length = htole32(blklen);

		ret = switchtec_cmd(dev, cmd_id, &cmd, sizeof(cmd),
				    NULL, 0);

		if (ret)
			return ret;

		ret = switchtec_fw_wait(dev, &status);
		if (ret != 0)
			return ret;

		offset += le32toh(cmd.hdr.blk_length);

		if (progress_callback)
			progress_callback(offset, image_size);

	}

	if (status == SWITCHTEC_DLSTAT_COMPLETES)
		return 0;

	if (status == SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT)
		return 0;

	if (status == SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT)
		return 0;

	if (status == 0)
		return SWITCHTEC_DLSTAT_HARDWARE_ERR;

	return status;
}

/**
 * @brief Write a firmware file to the switchtec device
 * @param[in] dev		Switchtec device handle
 * @param[in] fimg		FILE pointer for the image file to write
 * @param[in] dont_activate	If 1, the new image will not be activated
 * @param[in] force		If 1, ignore if another download command is
 *			        already in progress.
 * @param[in] progress_callback If not NULL, this function will be called to
 * 	indicate the progress.
 * @return 0 on success, error code on failure
 */
int switchtec_fw_write_file(struct switchtec_dev *dev, FILE *fimg,
			    int dont_activate, int force,
			    void (*progress_callback)(int cur, int tot))
{
	enum switchtec_fw_dlstatus status;
	enum mrpc_bg_status bgstatus;
	ssize_t image_size, offset = 0;
	int ret;
	struct cmd_fwdl cmd = {};
	uint32_t cmd_id = MRPC_FWDNLD;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW)
		cmd_id = MRPC_FW_TX;

	ret = fseek(fimg, 0, SEEK_END);
	if (ret)
		return -errno;
	image_size = ftell(fimg);
	if (image_size < 0)
		return -errno;
	ret = fseek(fimg, 0, SEEK_SET);
	if (ret)
		return -errno;

	switchtec_fw_dlstatus(dev, &status, &bgstatus);

	if (!force && status == SWITCHTEC_DLSTAT_INPROGRESS) {
		errno = EBUSY;
		return -EBUSY;
	}

	if (bgstatus == MRPC_BG_STAT_INPROGRESS) {
		errno = EBUSY;
		return -EBUSY;
	}

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2)
		cmd.hdr.subcmd = MRPC_FW_TX_FLASH;
	else
		cmd.hdr.subcmd = MRPC_FWDNLD_DOWNLOAD;

	cmd.hdr.dont_activate = !!dont_activate;
	cmd.hdr.img_length = htole32(image_size);

	while (offset < image_size) {
		ssize_t blklen = fread(&cmd.data, 1, sizeof(cmd.data), fimg);

		if (blklen == 0) {
			ret = ferror(fimg);
			if (ret)
				return ret;
			break;
		}

		cmd.hdr.offset = htole32(offset);
		cmd.hdr.blk_length = htole32(blklen);

		ret = switchtec_cmd(dev, cmd_id, &cmd, sizeof(cmd),
				    NULL, 0);

		if (ret)
			return ret;

		ret = switchtec_fw_wait(dev, &status);
		if (ret != 0)
			return ret;

		offset += le32toh(cmd.hdr.blk_length);

		if (progress_callback)
			progress_callback(offset, image_size);
	}

	if (status == SWITCHTEC_DLSTAT_COMPLETES)
		return 0;

	if (status == SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT)
		return 0;

	if (status == SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT)
		return 0;

	if (status == 0)
		return SWITCHTEC_DLSTAT_HARDWARE_ERR;

	return status;
}

/**
 * @brief Print an error string to stdout
 * @param[in] s		String that will be prefixed to the error message
 * @param[in] ret 	The value returned by the firmware function
 *
 * This can be called after Switchtec firmware function returned an error
 * to find out what caused the problem.
 */
void switchtec_fw_perror(const char *s, int ret)
{
	const char *msg;

	if (ret <= 0) {
		perror(s);
		return;
	}

	switch(ret) {
	case SWITCHTEC_DLSTAT_HEADER_INCORRECT:
		msg = "Header incorrect";  break;
	case SWITCHTEC_DLSTAT_OFFSET_INCORRECT:
		msg = "Offset incorrect";  break;
	case SWITCHTEC_DLSTAT_CRC_INCORRECT:
		msg = "CRC incorrect";  break;
	case SWITCHTEC_DLSTAT_LENGTH_INCORRECT:
		msg = "Length incorrect";  break;
	case SWITCHTEC_DLSTAT_HARDWARE_ERR:
		msg = "Hardware Error";  break;
	case SWITCHTEC_DLSTAT_DOWNLOAD_TIMEOUT:
		msg = "Download Timeout";  break;
	case SWITCHTEC_DLSTAT_NO_FILE:
		msg = "No Image Transferred"; break;
	default:
		fprintf(stderr, "%s: Unknown Error (0x%x)\n", s, ret);
		return;
	}

	fprintf(stderr, "%s: %s\n", s, msg);
}

static enum switchtec_fw_type
switchtec_fw_id_to_type_gen3(const struct switchtec_fw_image_info *info)
{
	switch ((unsigned long)info->part_id) {
	case SWITCHTEC_FW_PART_ID_G3_BOOT: return SWITCHTEC_FW_TYPE_BOOT;
	case SWITCHTEC_FW_PART_ID_G3_MAP0: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G3_MAP1: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G3_IMG0: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G3_IMG1: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G3_DAT0: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G3_DAT1: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G3_NVLOG: return SWITCHTEC_FW_TYPE_NVLOG;
	case SWITCHTEC_FW_PART_ID_G3_SEEPROM: return SWITCHTEC_FW_TYPE_SEEPROM;

	//Legacy
	case 0xa8000000: return SWITCHTEC_FW_TYPE_BOOT;
	case 0xa8020000: return SWITCHTEC_FW_TYPE_MAP;
	case 0xa8060000: return SWITCHTEC_FW_TYPE_IMG;
	case 0xa8210000: return SWITCHTEC_FW_TYPE_CFG;

	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

static enum switchtec_fw_type
switchtec_fw_id_to_type_gen4(const struct switchtec_fw_image_info *info)
{
	switch (info->part_id) {
	case SWITCHTEC_FW_PART_ID_G4_MAP0: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G4_MAP1: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G4_KEY0: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G4_KEY1: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G4_BL20: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G4_BL21: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G4_CFG0: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G4_CFG1: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G4_IMG0: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G4_IMG1: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G4_NVLOG: return SWITCHTEC_FW_TYPE_NVLOG;
	case SWITCHTEC_FW_PART_ID_G4_SEEPROM: return SWITCHTEC_FW_TYPE_SEEPROM;
	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

static enum switchtec_fw_type
switchtec_fw_id_to_type(const struct switchtec_fw_image_info *info)
{
	switch (info->gen) {
	case SWITCHTEC_GEN3: return switchtec_fw_id_to_type_gen3(info);
	case SWITCHTEC_GEN4: return switchtec_fw_id_to_type_gen4(info);
	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

static int switchtec_fw_file_info_gen3(int fd,
				       struct switchtec_fw_image_info *info)
{
	struct switchtec_fw_image_header_gen3 hdr = {};
	int ret;

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;

	if (strcmp(hdr.magic, "PMC") != 0)
		goto invalid_file;

	if (info == NULL)
		return 0;

	info->gen = SWITCHTEC_GEN3;
	info->part_id = hdr.type;
	info->image_crc = le32toh(hdr.image_crc);
	version_to_string(hdr.version, info->version, sizeof(info->version));
	info->image_len = le32toh(hdr.image_len);

	info->type = switchtec_fw_id_to_type(info);

	info->secure_version = 0;

	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}

static int switchtec_fw_file_info_gen4(int fd,
				       struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen4 hdr = {};

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;

	if (strncmp(hdr.magic, "MSCC", sizeof(hdr.magic)))
		goto invalid_file;

	if (strncmp(hdr.sub_magic, "_MD ", sizeof(hdr.sub_magic)))
		goto invalid_file;

	if (!info)
		return 0;

	info->gen = SWITCHTEC_GEN4;

	switch (le32toh(hdr.type)) {
	case SWITCHTEC_FW_IMG_TYPE_MAP_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_MAP0;
		break;
	case SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_KEY0;
		break;
	case SWITCHTEC_FW_IMG_TYPE_BL2_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_BL20;
		break;
	case SWITCHTEC_FW_IMG_TYPE_CFG_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_CFG0;
		break;
	case SWITCHTEC_FW_IMG_TYPE_IMG_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_IMG0;
		break;
	case SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_NVLOG;
		break;
	case SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN4:
		info->part_id = SWITCHTEC_FW_PART_ID_G4_SEEPROM;
		break;
	default:
		goto invalid_file;
	};

	info->image_crc = le32toh(hdr.image_crc);
	version_to_string(le32toh(hdr.version), info->version, sizeof(info->version));
	info->image_len = le32toh(hdr.image_len);

	info->type = switchtec_fw_id_to_type(info);

	info->secure_version = le32toh(hdr.secure_version);
	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}

/**
 * @brief Retrieve information about a firmware image file
 * @param[in]  fd	File descriptor for the image file to inspect
 * @param[out] info	Structure populated with information about the file
 * @return 0 on success, error code on failure
 */
int switchtec_fw_file_info(int fd, struct switchtec_fw_image_info *info)
{
	char magic[4];
	int ret;

	ret = read(fd, &magic, sizeof(magic));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(magic)) {
		errno = ENOEXEC;
		return -1;
	}

	if (!strncmp(magic, "PMC", sizeof(magic))) {
		return switchtec_fw_file_info_gen3(fd, info);
	} else if (!strncmp(magic, "MSCC", sizeof(magic))) {
		return switchtec_fw_file_info_gen4(fd, info);
	} else {
		errno = ENOEXEC;
		return -1;
	}

	return 0;
}

/**
 * @brief Check if the secure version of an image file is newer
 * 	  than that of the image on device.
 * @param[in]  dev	Switchtec device handle
 * @param[in]  img_fd	Image file descriptor
 * @return 1 if image file secure version > device secure version
 * 	   0 if image file secure version <= device secure version, or error
 */
int switchtec_fw_file_secure_version_newer(struct switchtec_dev *dev,
					   int img_fd)
{
	int ret;
	struct switchtec_fw_image_info info;
	struct switchtec_sn_ver_info sn_info = {};

	if (switchtec_is_gen3(dev))
		return 0;

	ret = switchtec_fw_file_info(img_fd, &info);
	if (ret)
		return 0;

	ret = switchtec_sn_ver_get(dev, &sn_info);
	if (ret) {
		sn_info.ver_bl2 = 0xffffffff;
		sn_info.ver_main = 0xffffffff;
		sn_info.ver_km = 0xffffffff;
	}

	switch (info.type) {
	case SWITCHTEC_FW_TYPE_BL2:
		if (info.secure_version > sn_info.ver_bl2)
			return 1;

		break;
	case SWITCHTEC_FW_TYPE_IMG:
		if (info.secure_version > sn_info.ver_main)
			return 1;

		break;
	case SWITCHTEC_FW_TYPE_KEY:
		if (info.secure_version > sn_info.ver_km) 
			return 1;

		break;
	default:
		break;
	}

	return 0;
}

/**
 * @brief Return a string describing the type of a firmware image
 * @param[out] info Information structure to return the type string for
 * @return Type string
 */
const char *switchtec_fw_image_type(const struct switchtec_fw_image_info *info)
{
	switch (info->type) {
	case SWITCHTEC_FW_TYPE_BOOT:	return "BOOT";
	case SWITCHTEC_FW_TYPE_MAP:	return "MAP";
	case SWITCHTEC_FW_TYPE_IMG:	return "IMG";
	case SWITCHTEC_FW_TYPE_CFG:	return "CFG";
	case SWITCHTEC_FW_TYPE_KEY:	return "KEY";
	case SWITCHTEC_FW_TYPE_BL2:	return "BL2";
	case SWITCHTEC_FW_TYPE_NVLOG:	return "NVLOG";
	case SWITCHTEC_FW_TYPE_SEEPROM:	return "SEEPROM";
	default:			return "UNKNOWN";
	}
}

static int switchtec_fw_map_get_active(struct switchtec_dev *dev,
				       struct switchtec_fw_image_info *info)
{
	uint32_t map0_update_index;
	uint32_t map1_update_index;
	int ret;

	ret = switchtec_fw_read(dev, SWITCHTEC_FLASH_MAP0_PART_START,
				sizeof(uint32_t), &map0_update_index);
	if (ret < 0)
		return ret;

	ret = switchtec_fw_read(dev, SWITCHTEC_FLASH_MAP1_PART_START,
				sizeof(uint32_t), &map1_update_index);
	if (ret < 0)
		return ret;

	info->active = 0;
	if (map0_update_index > map1_update_index) {
		if (info->part_addr == SWITCHTEC_FLASH_MAP0_PART_START)
			info->active = 1;
	} else {
		if (info->part_addr == SWITCHTEC_FLASH_MAP1_PART_START)
			info->active = 1;
	}

	return 0;
}

static int switchtec_fw_info_metadata_gen3(struct switchtec_dev *dev,
					   struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_footer_gen3 *metadata;
	unsigned long addr;
	int ret = 0;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G3_NVLOG)
		return 1;

	metadata = malloc(sizeof(*metadata));
	if (!metadata)
		return -1;

	addr = inf->part_addr + inf->part_len - sizeof(*metadata);

	ret = switchtec_fw_read(dev, addr, sizeof(*metadata), metadata);
	if (ret < 0)
		goto err_out;

	if (strncmp(metadata->magic, "PMC", sizeof(metadata->magic)))
		goto err_out;

	version_to_string(metadata->version, inf->version,
			  sizeof(inf->version));
	inf->part_body_offset = 0;
	inf->image_crc = metadata->image_crc;
	inf->image_len = metadata->image_len;
	inf->metadata = metadata;

	return 0;

err_out:
	free(metadata);
	return 1;
}

static int switchtec_fw_part_info_gen3(struct switchtec_dev *dev,
				       struct switchtec_fw_image_info *inf)
{
	int ret = 0;

	inf->read_only = switchtec_fw_is_boot_ro(dev);

	switch (inf->part_id) {
		case SWITCHTEC_FW_PART_ID_G3_BOOT:
			inf->part_addr = SWITCHTEC_FLASH_BOOT_PART_START;
			inf->part_len = SWITCHTEC_FLASH_PART_LEN;
			inf->active = true;
			break;
		case SWITCHTEC_FW_PART_ID_G3_MAP0:
			inf->part_addr = SWITCHTEC_FLASH_MAP0_PART_START;
			inf->part_len = SWITCHTEC_FLASH_PART_LEN;
			ret = switchtec_fw_map_get_active(dev, inf);
			break;
		case SWITCHTEC_FW_PART_ID_G3_MAP1:
			inf->part_addr = SWITCHTEC_FLASH_MAP1_PART_START;
			inf->part_len = SWITCHTEC_FLASH_PART_LEN;
			ret = switchtec_fw_map_get_active(dev, inf);
			break;
		default:
			ret = switchtec_flash_part(dev, inf, inf->part_id);
			inf->read_only = false;
	}

	if (ret)
		return ret;

	inf->valid = true;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G3_NVLOG)
		return 1;

	return switchtec_fw_info_metadata_gen3(dev, inf);
}

static int switchtec_fw_info_metadata_gen4(struct switchtec_dev *dev,
					   struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_metadata_gen4 *metadata;
	struct {
		uint8_t subcmd;
		uint8_t part_id;
	} subcmd = {
		.subcmd = MRPC_PART_INFO_GET_METADATA,
		.part_id = inf->part_id,
	};
	int ret;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G4_NVLOG)
		return 1;

	metadata = malloc(sizeof(*metadata));
	if (!metadata)
		return -1;

	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd, sizeof(subcmd),
			    metadata, sizeof(*metadata));
	if (ret)
		goto err_out;

	if (strncmp(metadata->magic, "MSCC", sizeof(metadata->magic)))
		goto err_out;

	if (strncmp(metadata->sub_magic, "_MD ", sizeof(metadata->sub_magic)))
		goto err_out;

	version_to_string(le32toh(metadata->version), inf->version,
			  sizeof(inf->version));
	inf->part_body_offset = le32toh(metadata->header_len);
	inf->image_crc = le32toh(metadata->image_crc);
	inf->image_len = le32toh(metadata->image_len);
	inf->metadata = metadata;

	return 0;

err_out:
	free(metadata);
	return -1;
}

struct switchtec_flash_info_gen4 {
	uint32_t firmware_version;
	uint32_t flash_size;
	uint16_t device_id;
	uint8_t ecc_enable;
	uint8_t rsvd1;
	uint8_t running_bl2_flag;
	uint8_t running_cfg_flag;
	uint8_t running_img_flag;
	uint8_t running_key_flag;
	uint8_t redundancy_key_flag;
	uint8_t redundancy_bl2_flag;
	uint8_t redundancy_cfg_flag;
	uint8_t redundancy_img_flag;
	uint32_t rsvd2[11];
	struct switchtec_flash_part_info_gen4  {
		uint32_t image_crc;
		uint32_t image_len;
		uint16_t image_version;
		uint8_t valid;
		uint8_t active;
		uint32_t part_start;
		uint32_t part_end;
		uint32_t part_offset;
		uint32_t part_size_dw;
		uint8_t read_only;
		uint8_t is_using;
		uint8_t rsvd[2];
	} map0, map1, keyman0, keyman1, bl20, bl21, cfg0, cfg1,
	  img0, img1, nvlog, vendor[8];
};

static int switchtec_fw_part_info_gen4(struct switchtec_dev *dev,
				       struct switchtec_fw_image_info *inf,
				       struct switchtec_flash_info_gen4 *all)
{
	struct switchtec_flash_part_info_gen4 *part_info;

	switch(inf->part_id) {
	case SWITCHTEC_FW_PART_ID_G4_MAP0:
		part_info = &all->map0;
		break;
	case SWITCHTEC_FW_PART_ID_G4_MAP1:
		part_info = &all->map1;
		break;
	case SWITCHTEC_FW_PART_ID_G4_KEY0:
		part_info = &all->keyman0;
		inf->redundant = all->redundancy_key_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_KEY1:
		part_info = &all->keyman1;
		inf->redundant = all->redundancy_key_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_BL20:
		part_info = &all->bl20;
		inf->redundant = all->redundancy_bl2_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_BL21:
		part_info = &all->bl21;
		inf->redundant = all->redundancy_bl2_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_IMG0:
		part_info = &all->img0;
		inf->redundant = all->redundancy_img_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_IMG1:
		part_info = &all->img1;
		inf->redundant = all->redundancy_img_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_CFG0:
		part_info = &all->cfg0;
		inf->redundant = all->redundancy_cfg_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_CFG1:
		part_info = &all->cfg1;
		inf->redundant = all->redundancy_cfg_flag;
		break;
	case SWITCHTEC_FW_PART_ID_G4_NVLOG:
		part_info = &all->nvlog;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	inf->part_addr = le32toh(part_info->part_start);
	inf->part_len = le32toh(part_info->part_size_dw) * 4;
	inf->active = part_info->active;
	inf->running = part_info->is_using;
	inf->read_only = part_info->read_only;
	inf->valid = part_info->valid;
	if (!inf->valid)
		return 0;

	return switchtec_fw_info_metadata_gen4(dev, inf);
}

/**
 * @brief Return firmware information structures for a number of firmware
 *	partitions.
 * @param[in]  dev	Switchtec device handle
 * @param[in]  nr_info 	Number of partitions to retrieve the info for
 * @param[out] info	Pointer to a list of info structs of at least
 *	\p nr_info entries
 * @return number of part info on success, negative on failure
 */
int switchtec_fw_part_info(struct switchtec_dev *dev, int nr_info,
				  struct switchtec_fw_image_info *info)
{
	int ret;
	int i;
	uint8_t subcmd = MRPC_PART_INFO_GET_ALL_INFO;
	struct switchtec_flash_info_gen4 all_info;

	if (info == NULL || nr_info == 0)
		return -EINVAL;

	if (dev->gen == SWITCHTEC_GEN4) {
		ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
				    sizeof(subcmd), &all_info,
				    sizeof(all_info));
		if (ret)
			return ret;
		all_info.firmware_version = le32toh(all_info.firmware_version);
		all_info.flash_size = le32toh(all_info.flash_size);
		all_info.device_id = le16toh(all_info.device_id);
	}

	for (i = 0; i < nr_info; i++) {
		struct switchtec_fw_image_info *inf = &info[i];
		ret = 0;

		inf->gen = dev->gen;
		inf->type = switchtec_fw_id_to_type(inf);
		inf->active = false;
		inf->running = false;
		inf->valid = false;

		switch (info->gen) {
		case SWITCHTEC_GEN3:
			ret = switchtec_fw_part_info_gen3(dev, inf);
			break;
		case SWITCHTEC_GEN4:
			ret = switchtec_fw_part_info_gen4(dev, inf, &all_info);
			break;
		default:
			errno = EINVAL;
			return -1;
		}

		if (ret < 0)
			return ret;

		if (ret) {
			inf->version[0] = 0;
			inf->image_crc = 0xFFFFFFFF;
			inf->metadata = NULL;
		}
	}

	return nr_info;
}

static long multicfg_subcmd(struct switchtec_dev *dev, uint32_t subcmd,
			    uint8_t index)
{
	int ret;
	uint32_t result;

	subcmd |= index << 8;
	subcmd = htole32(subcmd);

	ret = switchtec_cmd(dev, MRPC_MULTI_CFG, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));
	if (ret)
		return -1;

	return result;
}

static int get_multicfg(struct switchtec_dev *dev,
			struct switchtec_fw_image_info *info,
			int *nr_mult)
{
	int ret;
	int i;

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_SUPPORTED, 0);
	if (ret < 0)
		return ret;

	if (!ret) {
		*nr_mult = 0;
		return 0;
	}

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_COUNT, 0);
	if (ret < 0)
		return ret;

	if (*nr_mult > ret)
		*nr_mult = ret;

	for (i = 0; i < *nr_mult; i++) {
		info[i].part_addr = multicfg_subcmd(dev,
						    MRPC_MULTI_CFG_START_ADDR,
						    i);
		info[i].part_len = multicfg_subcmd(dev,
						   MRPC_MULTI_CFG_LENGTH, i);
		strcpy(info[i].version, "");
		info[i].image_crc = 0;
		info[i].active = 0;
	}

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_ACTIVE, 0);
	if (ret < 0)
		return ret;

	if (ret < *nr_mult)
		info[ret].active = 1;

	return 0;
}

static const enum switchtec_fw_image_part_id_gen3
switchtec_fw_partitions_gen3[] = {
	SWITCHTEC_FW_PART_ID_G3_BOOT,
	SWITCHTEC_FW_PART_ID_G3_MAP0,
	SWITCHTEC_FW_PART_ID_G3_MAP1,
	SWITCHTEC_FW_PART_ID_G3_IMG0,
	SWITCHTEC_FW_PART_ID_G3_DAT0,
	SWITCHTEC_FW_PART_ID_G3_DAT1,
	SWITCHTEC_FW_PART_ID_G3_NVLOG,
	SWITCHTEC_FW_PART_ID_G3_IMG1,
};

static const enum switchtec_fw_image_part_id_gen4
switchtec_fw_partitions_gen4[] = {
	SWITCHTEC_FW_PART_ID_G4_MAP0,
	SWITCHTEC_FW_PART_ID_G4_MAP1,
	SWITCHTEC_FW_PART_ID_G4_KEY0,
	SWITCHTEC_FW_PART_ID_G4_KEY1,
	SWITCHTEC_FW_PART_ID_G4_BL20,
	SWITCHTEC_FW_PART_ID_G4_BL21,
	SWITCHTEC_FW_PART_ID_G4_CFG0,
	SWITCHTEC_FW_PART_ID_G4_CFG1,
	SWITCHTEC_FW_PART_ID_G4_IMG0,
	SWITCHTEC_FW_PART_ID_G4_IMG1,
	SWITCHTEC_FW_PART_ID_G4_NVLOG,
};

static struct switchtec_fw_part_type *
switchtec_fw_type_ptr(struct switchtec_fw_part_summary *summary,
		      struct switchtec_fw_image_info *info)
{
	switch (info->type) {
	case SWITCHTEC_FW_TYPE_BOOT:	return &summary->boot;
	case SWITCHTEC_FW_TYPE_MAP:	return &summary->map;
	case SWITCHTEC_FW_TYPE_IMG:	return &summary->img;
	case SWITCHTEC_FW_TYPE_CFG:	return &summary->cfg;
	case SWITCHTEC_FW_TYPE_NVLOG:	return &summary->nvlog;
	case SWITCHTEC_FW_TYPE_SEEPROM: return &summary->seeprom;
	case SWITCHTEC_FW_TYPE_KEY:	return &summary->key;
	case SWITCHTEC_FW_TYPE_BL2:	return &summary->bl2;
	default:			return NULL;
	}
}

/**
 * @brief Return firmware summary information structure for the flash
 *	partitfons in the device
 * @param[in]  dev	Switchtec device handle
 *	\p nr_info entries
 * @return pointer to the structure on success, NULL on error. Free the
 *	the structure with \ref switchtec_fw_part_summary_free.
 */
struct switchtec_fw_part_summary *
switchtec_fw_part_summary(struct switchtec_dev *dev)
{
	struct switchtec_fw_part_summary *summary;
	struct switchtec_fw_image_info **infp;
	struct switchtec_fw_part_type *type;
	int nr_info, nr_mcfg = 16;
	size_t st_sz;
	int ret, i;

	switch (dev->gen) {
	case SWITCHTEC_GEN3:
		nr_info = ARRAY_SIZE(switchtec_fw_partitions_gen3);
		break;
	case SWITCHTEC_GEN4:
		nr_info = ARRAY_SIZE(switchtec_fw_partitions_gen4);
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	st_sz = sizeof(*summary) + sizeof(*summary->all) * (nr_info + nr_mcfg);

	summary = malloc(st_sz);
	if (!summary)
		return NULL;

	memset(summary, 0, st_sz);
	summary->nr_info = nr_info;

	switch (dev->gen) {
	case SWITCHTEC_GEN3:
		for (i = 0; i < nr_info; i++)
			summary->all[i].part_id =
				switchtec_fw_partitions_gen3[i];
		break;
	case SWITCHTEC_GEN4:
		for (i = 0; i < nr_info; i++)
			summary->all[i].part_id =
				switchtec_fw_partitions_gen4[i];
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	ret = switchtec_fw_part_info(dev, nr_info, summary->all);
	if (ret != nr_info) {
		free(summary);
		return NULL;
	}

	ret = get_multicfg(dev, &summary->all[nr_info], &nr_mcfg);
	if (ret) {
		nr_mcfg = 0;
		errno = 0;
	}

	for (i = 0; i < nr_info; i++) {
		type = switchtec_fw_type_ptr(summary, &summary->all[i]);
		if (type == NULL) {
			free(summary);
			return NULL;
		}
		if (summary->all[i].active)
			type->active = &summary->all[i];
		else
			type->inactive = &summary->all[i];
	}

	infp = &summary->mult_cfg;
	for (; i < nr_info + nr_mcfg; i++) {
		*infp = &summary->all[i];
		infp = &summary->all[i].next;
	}

	return summary;
}

/**
 * @brief Free a firmware part summary data structure
 * @param[in]  summary	The data structure to free.
 */
void switchtec_fw_part_summary_free(struct switchtec_fw_part_summary *summary)
{
	int i;

	for (i = 0; i < summary->nr_info; i++)
		free(summary->all[i].metadata);

	free(summary);
}

/**
 * @brief Read a Switchtec device's flash data
 * @param[in]  dev	Switchtec device handle
 * @param[in]  addr	Address to read from
 * @param[in]  len	Number of bytes to read
 * @param[out] buf	Destination buffer to read the data to
 * @return 0 on success, error code on failure
 */
int switchtec_fw_read(struct switchtec_dev *dev, unsigned long addr,
		      size_t len, void *buf)
{
	int ret;
	struct {
		uint32_t addr;
		uint32_t length;
	} cmd;
	unsigned char *cbuf = buf;
	size_t read = 0;

	while(len) {
		size_t chunk_len = len;
		if (chunk_len > MRPC_MAX_DATA_LEN-8)
			chunk_len = MRPC_MAX_DATA_LEN-8;

		cmd.addr = htole32(addr);
		cmd.length = htole32(chunk_len);

		ret = switchtec_cmd(dev, MRPC_RD_FLASH, &cmd, sizeof(cmd),
				    cbuf, chunk_len);
		if (ret)
			return -1;

		addr += chunk_len;
		len -= chunk_len;
		read += chunk_len;
		cbuf += chunk_len;
	}

	return read;
}

/**
 * @brief Read a Switchtec device's flash data into a file
 * @param[in] dev	Switchtec device handle
 * @param[in] fd	File descriptor of the file to save the firmware
 *	data to
 * @param[in] addr	Address to read from
 * @param[in] len	Number of bytes to read
 * @param[in] progress_callback This function is called periodically to
 *	indicate the progress of the read. May be NULL.
 * @return 0 on success, error code on failure
 */
int switchtec_fw_read_fd(struct switchtec_dev *dev, int fd,
			 unsigned long addr, size_t len,
			 void (*progress_callback)(int cur, int tot))
{
	int ret;
	unsigned char buf[(MRPC_MAX_DATA_LEN-8)*4];
	size_t read = 0;
	size_t total_len = len;
	size_t total_wrote;
	ssize_t wrote;

	while(len) {
		size_t chunk_len = len;
		if (chunk_len > sizeof(buf))
			chunk_len = sizeof(buf);

		ret = switchtec_fw_read(dev, addr, chunk_len, buf);
		if (ret < 0)
			return ret;

		total_wrote = 0;
		while (total_wrote < ret) {
			wrote = write(fd, &buf[total_wrote],
				      ret - total_wrote);
			if (wrote < 0)
				return -1;
			total_wrote += wrote;
		}

		read += ret;
		addr += ret;
		len -= ret;

		if (progress_callback)
			progress_callback(read, total_len);
	}

	return read;
}

/**
 * @brief Read a Switchtec device's flash image body into a file
 * @param[in]  dev     Switchtec device handle
 * @param[in]  fd      File descriptor for image file to write
 * @param[in]  info    Partition information structure
 * @param[in]  progress_callback This function is called periodically to
 *     indicate the progress of the read. May be NULL.
 * @return number of bytes written on success, error code on failure
 */
int switchtec_fw_body_read_fd(struct switchtec_dev *dev, int fd,
			      struct switchtec_fw_image_info *info,
			      void (*progress_callback)(int cur, int tot))
{
	return switchtec_fw_read_fd(dev, fd,
				    info->part_addr + info->part_body_offset,
				    info->image_len, progress_callback);
}

static int switchtec_fw_img_write_hdr_gen3(int fd,
		struct switchtec_fw_image_info *info)
{
	struct switchtec_fw_footer_gen3 *ftr = info->metadata;
	struct switchtec_fw_image_header_gen3 hdr = {};

	memcpy(hdr.magic, ftr->magic, sizeof(hdr.magic));
	hdr.image_len = ftr->image_len;
	hdr.type = info->part_id;
	hdr.load_addr = ftr->load_addr;
	hdr.version = ftr->version;
	hdr.header_crc = ftr->header_crc;
	hdr.image_crc = ftr->image_crc;

	if (hdr.type == SWITCHTEC_FW_PART_ID_G3_MAP1)
		hdr.type = SWITCHTEC_FW_PART_ID_G3_MAP0;
	else if (hdr.type == SWITCHTEC_FW_PART_ID_G3_IMG1)
		hdr.type = SWITCHTEC_FW_PART_ID_G3_IMG0;
	else if (hdr.type == SWITCHTEC_FW_PART_ID_G3_DAT1)
		hdr.type = SWITCHTEC_FW_PART_ID_G3_DAT0;

	return write(fd, &hdr, sizeof(hdr));
}

static int switchtec_fw_img_write_hdr_gen4(int fd,
		struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen4 *hdr = info->metadata;

	ret = write(fd, hdr, sizeof(*hdr));
	if (ret < 0)
		return ret;

	return lseek(fd, info->part_body_offset, SEEK_SET);
}

/**
 * @brief Write the header for a Switchtec firmware image file
 * @param[in]  fd	File descriptor for image file to write
 * @param[in]  info	Partition information structure
 * @return number of bytes written on success, error code on failure
 *
 * The offset of image body in the image file is greater than or equal to
 * the image header length. This function also repositions the read/write
 * file offset of fd to the offset of image body in the image file if
 * needed. This will facilitate the switchtec_fw_read_fd() function which
 * is usually called following this function to complete a firmware image
 * read.
 */
int switchtec_fw_img_write_hdr(int fd, struct switchtec_fw_image_info *info)
{
	switch (info->gen) {
	case SWITCHTEC_GEN3: return switchtec_fw_img_write_hdr_gen3(fd, info);
	case SWITCHTEC_GEN4: return switchtec_fw_img_write_hdr_gen4(fd, info);
	default:
		errno = EINVAL;
		return -1;
	}
}

struct switchtec_boot_ro {
	uint8_t subcmd;
	uint8_t set_get;
	uint8_t status;
	uint8_t reserved;
};

/**
 * @brief Check if the boot partition is marked as read-only
 * @param[in]  dev	Switchtec device handle
 * @return 1 if the partition is read-only, 0 if it's not or
 * 	a negative value if an error occurred
 */
int switchtec_fw_is_boot_ro(struct switchtec_dev *dev)
{
	struct switchtec_boot_ro subcmd = {
		.subcmd = MRPC_FWDNLD_BOOT_RO,
		.set_get = 0,
	};

	struct {
		uint8_t status;
		uint8_t reserved[3];
	} result;

	int ret;

	if (!switchtec_is_gen3(dev)) {
		errno = ENOTSUP;
		return -1;
	}

	ret = switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	if (ret == ERR_SUBCMD_INVALID) {
		errno = 0;
		return 0;
	}

	if (ret)
		return ret;

	return result.status;
}

/**
 * @brief Set or clear a boot partition's read-only flag
 * @param[in]  dev	Switchtec device handle
 * @param[in]  ro	Whether to set or clear the read-only flag
 * @return 0 on success, error code on failure
 */
int switchtec_fw_set_boot_ro(struct switchtec_dev *dev,
			     enum switchtec_fw_ro ro)
{
	struct switchtec_boot_ro subcmd = {
		.subcmd = MRPC_FWDNLD_BOOT_RO,
		.set_get = 1,
		.status = ro,
	};

	if (!switchtec_is_gen3(dev)) {
		errno = ENOTSUP;
		return -1;
	}

	return switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			     NULL, 0);
}

/**@}*/
