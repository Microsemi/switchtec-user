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

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

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

/**
 * @brief Perform an MRPC echo command
 * @param[in]  dev      Switchtec device handle
 * @param[out] status   The current download status
 * @param[out] bgstatus The current MRPC background status
 * @return 0 on success, error code on failure
 */
int switchtec_fw_dlstatus(struct switchtec_dev *dev,
			  enum switchtec_fw_dlstatus *status,
			  enum mrpc_bg_status *bgstatus)
{
	uint32_t subcmd = MRPC_FWDNLD_GET_STATUS;
	struct {
		uint8_t dlstatus;
		uint8_t bgstatus;
		uint16_t reserved;
	} result;
	int ret;

	ret = switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	if (ret < 0)
		return ret;

	if (status != NULL)
		*status = result.dlstatus;

	if (bgstatus != NULL)
		*bgstatus = result.bgstatus;

	return 0;
}

/**
 * @brief Wait for a firmware download chunk to complete
 * @param[in]  dev      Switchtec device handle
 * @param[out] status   The current download status
 * @return 0 on success, error code on failure
 *
 * Polls the firmware download status waiting until it no longer
 * indicates it's INPROGRESS. Sleeps 5ms between each poll.
 */
int switchtec_fw_wait(struct switchtec_dev *dev,
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
 * @param[in] toggle_fw  Set to 1 to toggle the main FW image
 * @param[in] toggle_cfg Set to 1 to toggle the config FW image
 * @return 0 on success, error code on failure
 */
int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_bl2, int toggle_keyman,
					 int toggle_fw, int toggle_cfg)
{
	struct {
		uint8_t subcmd;
		uint8_t toggle_fw;
		uint8_t toggle_cfg;
		uint8_t toggle_bl2;
		uint8_t toggle_keyman;
	} cmd;

	cmd.subcmd = MRPC_FWDNLD_TOGGLE;
	cmd.toggle_bl2 = !!toggle_bl2;
	cmd.toggle_keyman = !!toggle_keyman;
	cmd.toggle_fw = !!toggle_fw;
	cmd.toggle_cfg = !!toggle_cfg;

	return switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd),
			     NULL, 0);
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

		ret = switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd),
				    NULL, 0);

		if (ret < 0)
			return ret;

		ret = switchtec_fw_wait(dev, &status);
		if (ret != 0)
			return ret;

		offset += cmd.hdr.blk_length;

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

		ret = switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd),
				    NULL, 0);

		if (ret < 0)
			return ret;

		ret = switchtec_fw_wait(dev, &status);
		if (ret != 0)
			return ret;

		offset += cmd.hdr.blk_length;

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
	default:
		fprintf(stderr, "%s: Unknown Error (%d)\n", s, ret);
		return;
	}

	fprintf(stderr, "%s: %s\n", s, msg);
}

struct fw_image_header_gen3 {
	char magic[4];
	uint32_t image_len;
	uint32_t type;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd[9];
	uint32_t header_crc;
	uint32_t image_crc;
};

struct fw_metadata_gen4 {
	char magic[4];
	char sub_magic[4];
	uint32_t hdr_version;
	uint32_t secure_version;
	uint32_t header_len;
	uint32_t metadata_len;
	uint32_t image_len;
	uint32_t type;
	uint32_t rsvd;
	uint32_t version;
	uint32_t sequence;
	uint32_t reserved1;
	uint8_t date_str[8];
	uint8_t time_str[8];
	uint8_t img_str[16];
	uint8_t rsvd1[4];
	uint32_t image_crc;
	uint8_t public_key_modulus[512];
	uint8_t public_key_exponent[4];
	uint8_t uart_port;
	uint8_t uart_rate;
	uint8_t bist_enable;
	uint8_t bist_gpio_pin_cfg;
	uint8_t bist_gpio_level_cfg;
	uint8_t rsvd2[3];
	uint32_t xml_version;
	uint32_t relocatable_img_len;
	uint32_t link_addr;
	uint32_t header_crc;
};

struct fw_image_header {
	union {
		char magic[4];
		struct fw_image_header_gen3 gen3;
		struct fw_metadata_gen4 gen4;
	};
};

static enum switchtec_fw_partition_type flash_part_type(
		struct fw_image_header *hdr)
{
	if (strncmp(hdr->magic, "PMC", sizeof(hdr->magic)) == 0) {
		switch(hdr->gen3.type) {
		case SWITCHTEC_FW_PART_ID_BOOT_GEN3:
			return SWITCHTEC_FW_PART_TYPE_BOOT;
		case SWITCHTEC_FW_PART_ID_MAP0_GEN3:
			return SWITCHTEC_FW_PART_TYPE_MAP;
		case SWITCHTEC_FW_PART_ID_MAP1_GEN3:
			return SWITCHTEC_FW_PART_TYPE_MAP;
		case SWITCHTEC_FW_PART_ID_IMG0_GEN3:
			return SWITCHTEC_FW_PART_TYPE_IMG;
		case SWITCHTEC_FW_PART_ID_IMG1_GEN3:
			return SWITCHTEC_FW_PART_TYPE_IMG;
		case SWITCHTEC_FW_PART_ID_CFG0_GEN3:
			return SWITCHTEC_FW_PART_TYPE_CFG;
		case SWITCHTEC_FW_PART_ID_CFG1_GEN3:
			return SWITCHTEC_FW_PART_TYPE_CFG;
		case SWITCHTEC_FW_PART_ID_NVLOG_GEN3:
			return SWITCHTEC_FW_PART_TYPE_NVLOG;
		case SWITCHTEC_FW_PART_TYPE_SEEPROM_GEN3:
			return SWITCHTEC_FW_PART_TYPE_SEEPROM;
		default:
			return SWITCHTEC_FW_PART_TYPE_UNKNOWN;
		}
	} else if (strncmp(hdr->magic, "MSCC", sizeof(hdr->magic)) == 0 &&
		   strncmp(hdr->gen4.sub_magic, "_MD ",
			   sizeof(hdr->gen4.sub_magic)) == 0) {
		switch(hdr->gen4.type) {
		case SWITCHTEC_FW_PART_TYPE_MAP_GEN4:
			return SWITCHTEC_FW_PART_TYPE_MAP;
		case SWITCHTEC_FW_PART_TYPE_KEYMAN_GEN4:
			return SWITCHTEC_FW_PART_TYPE_KEYMAN;
		case SWITCHTEC_FW_PART_TYPE_BL2_GEN4:
			return SWITCHTEC_FW_PART_TYPE_BL2;
		case SWITCHTEC_FW_PART_TYPE_CFG_GEN4:
			return SWITCHTEC_FW_PART_TYPE_CFG;
		case SWITCHTEC_FW_PART_TYPE_IMG_GEN4:
			return SWITCHTEC_FW_PART_TYPE_IMG;
		case SWITCHTEC_FW_PART_TYPE_NVLOG_GEN4:
			return SWITCHTEC_FW_PART_TYPE_NVLOG;
		case SWITCHTEC_FW_PART_TYPE_SEEPROM_GEN4:
			return SWITCHTEC_FW_PART_TYPE_SEEPROM;
		default:
			return SWITCHTEC_FW_PART_TYPE_UNKNOWN;
		}
	}

	return SWITCHTEC_FW_PART_TYPE_UNKNOWN;
}

/**
 * @brief Retrieve information about a firmware image file
 * @param[in]  fd	File descriptor for the image file to inspect
 * @param[out] info	Structure populated with information about the file
 * @return 0 on success, error code on failure
 */
int switchtec_fw_image_file_info(int fd,
				 struct switchtec_fw_partition_info *info)
{
	int ret;
	int size;

	if (info == NULL)
		return 0;

	struct fw_image_header hdr = {};
	size = offsetof(struct fw_image_header, magic) + sizeof(hdr.magic);
	ret = read(fd, &hdr, size);
	lseek(fd, 0, SEEK_SET);
	if (ret != size)
		goto invalid_file;

	if (strncmp(hdr.magic, "PMC", sizeof(hdr.magic)) == 0) {
		size = offsetof(struct fw_image_header, gen3) +
		       sizeof(hdr.gen3);
		ret = read(fd, &hdr, size);
		lseek(fd, 0, SEEK_SET);
		if (ret != size)
			goto invalid_file;

		info->type = flash_part_type(&hdr);
		info->image_crc = le32toh(hdr.gen3.image_crc);
		version_to_string(hdr.gen3.version, info->ver_str,
				  sizeof(info->ver_str));
		info->image_len = le32toh(hdr.gen3.image_len);
	} else if (strncmp(hdr.magic, "MSCC", sizeof(hdr.magic)) == 0) {
		lseek(fd, offsetof(struct fw_metadata_gen4, sub_magic),
				   SEEK_SET);

		size = sizeof(hdr.gen4.sub_magic);
		ret = read(fd, hdr.gen4.sub_magic, size);
		if (ret != size)
			goto invalid_file;

		if (strncmp(hdr.gen4.sub_magic, "_MD ",
			    sizeof(hdr.gen4.sub_magic)) != 0)
			goto invalid_file;

		lseek(fd, 0, SEEK_SET);

		size = offsetof(struct fw_image_header, gen4) +
		       sizeof(hdr.gen4);
		ret = read(fd, &hdr, size);
		lseek(fd, 0, SEEK_SET);
		if (ret != size)
			goto invalid_file;
		info->type = flash_part_type(&hdr);
		info->image_crc = le32toh(hdr.gen4.image_crc);
		version_to_string(hdr.gen4.version, info->ver_str,
				  sizeof(info->ver_str));
		info->image_len = le32toh(hdr.gen4.image_len);
	} else {
		goto invalid_file;
	}

	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}

/**
 * @brief Return a string describing the type of a firmware image
 * @param[out] info Information structure to return the type string for
 * @return Type string
 */
const char *switchtec_fw_part_type(
		const struct switchtec_fw_partition_info *info)
{
	switch((unsigned long)info->type) {
	case SWITCHTEC_FW_PART_TYPE_BOOT: return "BOOT";
	case SWITCHTEC_FW_PART_TYPE_MAP: return "MAP";
	case SWITCHTEC_FW_PART_TYPE_BL2: return "BL2";
	case SWITCHTEC_FW_PART_TYPE_KEYMAN: return "KEYMAN";
	case SWITCHTEC_FW_PART_TYPE_IMG: return "IMG";
	case SWITCHTEC_FW_PART_TYPE_CFG: return "DAT";
	case SWITCHTEC_FW_PART_TYPE_NVLOG: return "NVLOG";
	case SWITCHTEC_FW_PART_TYPE_SEEPROM: return "SEEPROM";
	default: return "UNKNOWN";
	}
}

static long multicfg_subcmd(struct switchtec_dev *dev, uint32_t subcmd,
			    uint8_t index)
{
	int ret;
	uint32_t result;

	subcmd |= index << 8;

	ret = switchtec_cmd(dev, MRPC_MULTI_CFG, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));
	if (ret)
		return -1;

	return result;
}

struct flash_part_info_gen4 {
	uint32_t image_crc;
	uint32_t image_len;
	uint16_t image_version;
	uint8_t valid;
	uint8_t active;
	uint32_t part_start;
	uint32_t part_end;
	uint32_t part_offset;
	uint32_t part_size_dw;
	uint8_t readonly;
	uint8_t is_using;
	uint8_t rsvd[2];
};

struct flash_part_all_info_gen4 {
	uint32_t firmware_version;
	uint32_t flash_size;
	uint8_t ecc_enable;
	uint8_t rsvd1;
	uint8_t running_bl2_flag;
	uint8_t running_cfg_flag;
	uint8_t running_img_flag;
	uint8_t rsvd2;
	uint32_t rsvd3[12];
	struct flash_part_info_gen4 map0;
	struct flash_part_info_gen4 map1;
	struct flash_part_info_gen4 keyman0;
	struct flash_part_info_gen4 keyman1;
	struct flash_part_info_gen4 bl20;
	struct flash_part_info_gen4 bl21;
	struct flash_part_info_gen4 cfg0;
	struct flash_part_info_gen4 cfg1;
	struct flash_part_info_gen4 img0;
	struct flash_part_info_gen4 img1;
	struct flash_part_info_gen4 nvlog;
	struct flash_part_info_gen4 vendor[8];
};

int switchtec_fw_get_multicfg(struct switchtec_dev *dev,
			      struct switchtec_fw_partition_info *info,
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
		info[i].version = 0;
		info[i].image_crc = 0;
		info[i].valid = 1;
		info[i].active = 0;
		info[i].running = 0;
		info[i].readonly = 0;
	}

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_ACTIVE, 0);
	if (ret < 0)
		return ret;

	if (ret < *nr_mult)
		info[ret].active = 1;

	return 0;
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

struct fw_footer_gen3 {
	char magic[4];
	uint32_t image_len;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd;
	uint32_t header_crc;
	uint32_t image_crc;
};


/**
 * @brief Read a Switchtec device's firmware partition footer
 * @param[in]  dev		Switchtec device handle
 * @param[in]  partition_start	Partition start address
 * @param[in]  partition_len	Partition length
 * @param[out] ftr		The footer structure to populate
 * @param[out] version		Optional pointer to a string which will
 *	be populated with a human readable string of the version
 * @param[in]  version_len	Maximum length of the version string
 * @return 0 on success, error code on failure
 */
static int fw_read_footer_gen3(struct switchtec_dev *dev,
			       unsigned long partition_start,
			       size_t partition_len,
			       struct fw_footer_gen3 *ftr,
			       char *version, size_t version_len)
{
	int ret;
	unsigned long addr = partition_start + partition_len -
		sizeof(struct fw_footer_gen3);

	if (!ftr)
		return -EINVAL;

	ret = switchtec_fw_read(dev, addr, sizeof(struct fw_footer_gen3),
				ftr);
	if (ret < 0)
		return ret;

	if (strcmp(ftr->magic, "PMC") != 0) {
		errno = ENOEXEC;
		return -errno;
	}

	if (version)
		version_to_string(ftr->version, version, version_len);

	return 0;
}

static int fw_read_flash_all_info_gen4(struct switchtec_dev *dev,
				       struct flash_part_all_info_gen4 *info)
{
	uint8_t subcmd = MRPC_FLASH_GET_ALL_INFO;

	return switchtec_cmd(dev, MRPC_PART_INFO, &subcmd, sizeof(subcmd),
			     info, sizeof(*info));
}

static int set_flash_part_info(struct switchtec_fw_partition_info *info,
			       enum switchtec_fw_partition_id part_id,
			       uint32_t part_addr, uint32_t part_len,
			       uint32_t image_len,
			       int valid, int active, int running, int readonly,
			       uint32_t crc, uint32_t version)
{
	info->part_id = part_id;
	info->part_addr = part_addr;
	info->part_len = part_len;
	info->image_len = image_len;
	info->valid = valid;
	info->running = running;
	info->readonly = readonly;
	info->active = active;
	info->image_crc = crc;
	info->version = version;

	return 0;
}

static int fw_flash_part_info_gen3(struct switchtec_dev *dev,
				   enum switchtec_fw_partition_id id,
				   struct switchtec_fw_partition_info *info)
{
	int ret;
	struct fw_footer_gen3 ftr;
	int img_cfg_nvlog = 0;
	size_t part_start;
	size_t part_len;

	info->valid = 1;
	info->readonly = 0;
	info->running = 0;
	info->active = 0;

	switch(id) {
	case SWITCHTEC_FW_PART_ID_BOOT:
		part_start = SWITCHTEC_FLASH_BOOT_PART_START;
		part_len = SWITCHTEC_FLASH_PART_LEN;

		info->readonly = switchtec_fw_is_boot_ro(dev);
		if (info->readonly != SWITCHTEC_FW_RO)
			info->readonly = 0;
		info->part_id = SWITCHTEC_FW_PART_ID_BOOT;
		info->type = SWITCHTEC_FW_PART_TYPE_BOOT;
		break;
	case SWITCHTEC_FW_PART_ID_MAP0:
		part_start = SWITCHTEC_FLASH_MAP0_PART_START;
		part_len = SWITCHTEC_FLASH_PART_LEN;
		info->part_id = SWITCHTEC_FW_PART_ID_MAP0;
		info->type = SWITCHTEC_FW_PART_TYPE_MAP;
		break;
	case SWITCHTEC_FW_PART_ID_MAP1:
		part_start = SWITCHTEC_FLASH_MAP1_PART_START;
		part_len = SWITCHTEC_FLASH_PART_LEN;
		info->part_id = SWITCHTEC_FW_PART_ID_MAP1;
		info->type = SWITCHTEC_FW_PART_TYPE_MAP;
		break;
	case SWITCHTEC_FW_PART_ID_IMG0:
		img_cfg_nvlog = 1;
		info->part_id = SWITCHTEC_FW_PART_ID_IMG0;
		info->type = SWITCHTEC_FW_PART_TYPE_IMG;
		break;
	case SWITCHTEC_FW_PART_ID_IMG1:
		img_cfg_nvlog = 1;
		info->part_id = SWITCHTEC_FW_PART_ID_IMG1;
		info->type = SWITCHTEC_FW_PART_TYPE_IMG;
		break;
	case SWITCHTEC_FW_PART_ID_CFG0:
		img_cfg_nvlog = 1;
		info->part_id = SWITCHTEC_FW_PART_ID_CFG0;
		info->type = SWITCHTEC_FW_PART_TYPE_CFG;
		break;
	case SWITCHTEC_FW_PART_ID_CFG1:
		img_cfg_nvlog = 1;
		info->part_id = SWITCHTEC_FW_PART_ID_CFG1;
		info->type = SWITCHTEC_FW_PART_TYPE_CFG;
		break;
	case SWITCHTEC_FW_PART_ID_NVLOG:
		img_cfg_nvlog = 1;
		info->part_id = SWITCHTEC_FW_PART_ID_NVLOG;
		info->type = SWITCHTEC_FW_PART_TYPE_NVLOG;
		info->ver_str[0] = 0;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if (img_cfg_nvlog) {
		ret = switchtec_flash_part(dev, info, info->part_id);
		if (ret)
			return ret;

		if (switchtec_fw_active(info))
			info->active = 1;

		if (switchtec_fw_running(info))
			info->running = 1;

		part_start = info->part_addr;
		part_len = info->part_len;
	}

	ret = fw_read_footer_gen3(dev, part_start, part_len, &ftr,
				  info->ver_str, sizeof(info->ver_str));

	info->part_addr = part_start;
	info->part_len = part_len;

	info->image_len = ftr.image_len;
	info->image_crc = ftr.image_crc;

	return 0;
}

static int fw_flash_part_info_gen4(struct switchtec_dev *dev,
				   enum switchtec_fw_partition_id id,
				   struct switchtec_fw_partition_info *info)
{
	struct flash_part_all_info_gen4 all_info;
	struct flash_part_info_gen4 *part_info;
	struct switchtec_fw_metadata meta;
	int ret;

	ret = fw_read_flash_all_info_gen4(dev, &all_info);
	if (ret < 0)
		return ret;

	switch(id) {
	case SWITCHTEC_FW_PART_ID_MAP0:
		part_info = &all_info.map0;
		info->type = SWITCHTEC_FW_PART_TYPE_MAP;
		break;
	case SWITCHTEC_FW_PART_ID_MAP1:
		part_info = &all_info.map1;
		info->type = SWITCHTEC_FW_PART_TYPE_MAP;
		break;
	case SWITCHTEC_FW_PART_ID_KEYMAN0:
		part_info = &all_info.keyman0;
		info->type = SWITCHTEC_FW_PART_TYPE_KEYMAN;
		break;
	case SWITCHTEC_FW_PART_ID_KEYMAN1:
		part_info = &all_info.keyman1;
		info->type = SWITCHTEC_FW_PART_TYPE_KEYMAN;
		break;
	case SWITCHTEC_FW_PART_ID_BL20:
		part_info = &all_info.bl20;
		info->type = SWITCHTEC_FW_PART_TYPE_BL2;
		break;
	case SWITCHTEC_FW_PART_ID_BL21:
		part_info = &all_info.bl21;
		info->type = SWITCHTEC_FW_PART_TYPE_BL2;
		break;
	case SWITCHTEC_FW_PART_ID_IMG0:
		part_info = &all_info.img0;
		info->type = SWITCHTEC_FW_PART_TYPE_IMG;
		break;
	case SWITCHTEC_FW_PART_ID_IMG1:
		part_info = &all_info.img1;
		info->type = SWITCHTEC_FW_PART_TYPE_IMG;
		break;
	case SWITCHTEC_FW_PART_ID_CFG0:
		part_info = &all_info.cfg0;
		info->type = SWITCHTEC_FW_PART_TYPE_CFG;
		break;
	case SWITCHTEC_FW_PART_ID_CFG1:
		part_info = &all_info.cfg1;
		info->type = SWITCHTEC_FW_PART_TYPE_CFG;
		break;
	case SWITCHTEC_FW_PART_ID_NVLOG:
		part_info = &all_info.nvlog;
		info->type = SWITCHTEC_FW_PART_TYPE_NVLOG;
		info->ver_str[0] = 0;
		set_flash_part_info(info, id,
				    part_info->part_start,
				    part_info->part_size_dw * 4,
				    part_info->part_size_dw * 4,
				    1, 0, 0, 0, 0, 0);
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}

	ret = switchtec_fw_read_metadata(dev, id, &meta,
					 info->ver_str,
					 sizeof(info->ver_str));
	if (ret < 0)
		return ret;

	set_flash_part_info(info, id,
			    part_info->part_start,
			    part_info->part_size_dw * 4,
			    meta.image_len,
			    part_info->valid, part_info->active,
			    part_info->is_using, part_info->readonly,
			    meta.image_crc, meta.version);
	return 0;
}

int switchtec_fw_partition_info(struct switchtec_dev *dev,
				enum switchtec_fw_partition_id id,
				struct switchtec_fw_partition_info *info)
{
	if (!info)
		return 0;

	if (switchtec_is_gen3(dev)) {
		return fw_flash_part_info_gen3(dev, id, info);
	} else if (switchtec_is_gen4(dev)) {
		return fw_flash_part_info_gen4(dev, id, info);
	}

	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Read a Switchtec device's firmware partition metadata
 * @param[in]  dev		Switchtec device handle
 * @param[in]  part_id		Flash partition ID
 * @param[out] meta		The metadata structure to populate
 * @param[out] version		Optional pointer to a string which will
 *	be populated with a human readable string of the version
 * @param[in]  version_len	Maximum length of the version string
 * @return 0 on success, error code on failure
 */
int switchtec_fw_read_metadata(struct switchtec_dev *dev,
			       enum switchtec_fw_partition_id part_id,
			       struct switchtec_fw_metadata *meta,
			       char *version, size_t version_len)
{
	int ret;

	if (!meta)
		return -EINVAL;

	if (switchtec_is_gen3(dev)) {
		struct switchtec_fw_partition_info inf;
		struct fw_footer_gen3 ftr;
		unsigned long addr;

		if (part_id == SWITCHTEC_FW_PART_ID_MAP0)
			addr = SWITCHTEC_FLASH_MAP0_PART_START +
			       SWITCHTEC_FLASH_PART_LEN -
			       sizeof(struct fw_footer_gen3);
		else if (part_id == SWITCHTEC_FW_PART_ID_MAP1)
			addr = SWITCHTEC_FLASH_MAP1_PART_START +
			       SWITCHTEC_FLASH_PART_LEN -
			       sizeof(struct fw_footer_gen3);
		else if (part_id == SWITCHTEC_FW_PART_ID_BOOT)
			addr = SWITCHTEC_FLASH_BOOT_PART_START +
			       SWITCHTEC_FLASH_PART_LEN -
			       sizeof(struct fw_footer_gen3);
		else {
			inf.type = GEN3_FW_PART_ID(part_id);
			ret = switchtec_flash_part(dev, &inf, part_id);
			if (ret)
				return ret;

			addr = inf.part_addr + inf.part_len -
			       sizeof(struct fw_footer_gen3);
		}
		ret = switchtec_fw_read(dev, addr, sizeof(ftr), &ftr);
		if (ret < 0)
			return ret;

		if (strncmp(ftr.magic, "PMC", sizeof(ftr.magic)) != 0) {
			errno = ENOEXEC;
			return -errno;
		}

		strncpy(meta->magic, "PMC", sizeof(meta->magic));
		meta->load_addr = ftr.load_addr;
		meta->version = ftr.version;
		meta->image_len = ftr.image_len;
		meta->header_crc = ftr.header_crc;
		meta->image_crc = ftr.image_crc;
	} else if (switchtec_is_gen4(dev)) {
		struct fw_metadata_gen4 fw_meta;

		struct {
			uint8_t subcmd;
			uint8_t part_id;
		} subcmd = {
			.subcmd = MRPC_FLASH_GET_METADATA,
			.part_id = GEN4_FW_PART_ID(part_id),
		};

		ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
				    sizeof(subcmd), &fw_meta, sizeof(fw_meta));

		if (ret)
			return ret;

		if (strncmp(fw_meta.magic, "MSCC",
			    sizeof(fw_meta.magic)) != 0 ||
		    strncmp(fw_meta.sub_magic, "_MD ",
			    sizeof(fw_meta.sub_magic)) != 0) {
			errno = ENOEXEC;
			return -errno;
		}

		memcpy(meta->magic, "MSCC", sizeof(meta->magic));
		memcpy(meta->sub_magic, "_MD ", sizeof(meta->sub_magic));
		meta->type = fw_meta.type;
		meta->version = fw_meta.version;
		meta->secure_version = fw_meta.secure_version;
		meta->sequence = fw_meta.sequence;
		meta->uart_port = fw_meta.uart_port;
		meta->uart_rate = fw_meta.uart_rate;
		meta->bist_enable = fw_meta.bist_enable;
		meta->bist_gpio_pin_cfg = fw_meta.bist_gpio_pin_cfg;
		meta->bist_gpio_level_cfg = fw_meta.bist_gpio_level_cfg;
		meta->xml_version = fw_meta.xml_version;
		meta->relocatable_img_len = fw_meta.relocatable_img_len;
		meta->link_addr = fw_meta.link_addr;
		memcpy(meta->date_str, fw_meta.date_str,
		       sizeof(meta->date_str));
		memcpy(meta->time_str, fw_meta.time_str,sizeof(meta->time_str));
		memcpy(meta->img_str, fw_meta.img_str, sizeof(meta->img_str));
		memcpy(meta->public_key_modulus, fw_meta.public_key_modulus,
		       sizeof(meta->public_key_modulus));
		memcpy(meta->public_key_exponent, fw_meta.public_key_exponent,
		       sizeof(meta->public_key_exponent));
		meta->image_len = fw_meta.image_len;
		meta->header_crc = fw_meta.header_crc;
		meta->image_crc = fw_meta.image_crc;
	} else {
		errno = ENOTSUP;
		return -1;
	}

	if (version)
		version_to_string(meta->version, version, version_len);

	return 0;
}

/**
 * @brief Write the header for a Switchtec firmware image file
 * @param[in]  fd	File descriptor for image file to write
 * @param[in]  ftr	Footer information to include in the header
 * @param[in]  type	File type to record in the header
 * @return 0 on success, error code on failure
 */
int switchtec_fw_img_file_write_hdr(int fd, struct switchtec_fw_metadata *meta,
				    enum switchtec_fw_partition_type type)
{
	struct fw_image_header hdr = {};

	memcpy(hdr.magic, meta->magic, sizeof(hdr.magic));

	if (!strncmp(hdr.magic, "PMC", sizeof(hdr.magic))) {
		hdr.gen3.image_len = meta->image_len;
	        hdr.gen3.type = GEN3_FW_PART_TYPE(type);
	        hdr.gen3.load_addr = meta->load_addr;
	        hdr.gen3.version = meta->version;
	        hdr.gen3.header_crc = meta->header_crc;
	        hdr.gen3.image_crc = meta->image_crc;

	        return write(fd, &hdr,
			     offsetof(struct fw_image_header, gen3) +
			     sizeof(hdr.gen3));
	} else if (!strncmp(hdr.magic, "MSCC", sizeof(hdr.magic)) &&
		   !strncmp(meta->sub_magic, "_MD ",
			    sizeof(hdr.gen4.sub_magic))) {
		memcpy(hdr.gen4.sub_magic, meta->sub_magic,
		       sizeof(hdr.gen4.sub_magic));
		hdr.gen4.image_len = meta->image_len;
		hdr.gen4.type = GEN4_FW_PART_TYPE(type);
		hdr.gen4.version = meta->version;
		hdr.gen4.secure_version = meta->secure_version;
		hdr.gen4.sequence = meta->sequence;
		hdr.gen4.uart_port = meta->uart_port;
		hdr.gen4.uart_rate = meta->uart_rate;
		hdr.gen4.bist_enable = meta->bist_enable;
		hdr.gen4.bist_gpio_pin_cfg = meta->bist_gpio_pin_cfg;
		hdr.gen4.bist_gpio_level_cfg = meta->bist_gpio_level_cfg;
		hdr.gen4.xml_version = meta->xml_version;
		hdr.gen4.relocatable_img_len = meta->relocatable_img_len;
		hdr.gen4.link_addr = meta->link_addr;
		memcpy(hdr.gen4.date_str, meta->date_str,
		       sizeof(hdr.gen4.date_str));
		memcpy(hdr.gen4.time_str, meta->time_str,
		       sizeof(hdr.gen4.time_str));
		memcpy(hdr.gen4.img_str, meta->img_str,
		       sizeof(hdr.gen4.img_str));
		memcpy(hdr.gen4.public_key_modulus, meta->public_key_modulus,
		       sizeof(hdr.gen4.public_key_modulus));
		memcpy(hdr.gen4.public_key_exponent, meta->public_key_exponent,
		       sizeof(hdr.gen4.public_key_exponent));
		hdr.gen4.image_len = meta->image_len;
		hdr.gen4.header_crc = meta->header_crc;
		hdr.gen4.image_crc = meta->image_crc;

		return write(fd, &hdr,
			     offsetof(struct fw_image_header, gen4) +
			     sizeof(hdr.gen4));
	}

	errno = ENOTSUP;
	return -1;
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

	return switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			     NULL, 0);
}

/**@}*/
