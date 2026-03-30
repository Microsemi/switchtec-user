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

#include "gen4/fw_gen4.h"
#include "gen5/fw_gen5.h"
#include "gen6/fw_gen6.h"

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
 * @brief Set the redundant image flag for the specified image types
 * @param[in] dev        Switchtec device handle
 * @param[in] toggle_arr Pointer to the array of togglable image types
 * @return 0 on success, error code on failure
 */
int switchtec_fw_set_redundant_flag (struct switchtec_dev *dev, int keyman,
				     int riot, int bl2, int cfg, int fw,
				     int set)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_set_redundant_flag)
		return GEN_OPS(dev)->fw_set_redundant_flag(dev, keyman, riot, bl2, cfg, fw, set);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Download fwimg file from device
 * @brief Write a firmware file to the switchtec device
 * @param[in] dev		Switchtec device handle
 * @param[in] fd		File descriptor for the image file to write
 * @param[in] fw_type		Firmware type to download
 * @param[in] fw_slot		Firmware slot to download
 * @param[in] progress_callback If not NULL, this function will be called to
 * 	indicate the progress.
 * @return 0 on success, error code on failure
 *
 * The fw_img_get command will download the firmware image corresponding to fw type and slot 
 * from the device to the file descriptor provided.
 */
int switchtec_fw_img_get(struct switchtec_dev *dev, int fd,
			 enum switchtec_fw_type_gen6 fw_type, int fw_slot,
			 void (*progress_callback)(int cur, int tot))
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_img_get)
		return GEN_OPS(dev)->fw_img_get(dev, fd, fw_type, fw_slot, progress_callback);
	errno = ENOTSUP;
	return -1;
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
					 int toggle_fw, int toggle_cfg,
					 int toggle_riotcore)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_toggle_active_partition)
		return GEN_OPS(dev)->fw_toggle_active_partition(dev, toggle_bl2, toggle_key, toggle_fw, toggle_cfg, toggle_riotcore);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Extract generation information from FW version number
 * @param[in] version		Firmware version number
 * @return Generation information contained in the FW version number
 */
enum switchtec_gen switchtec_fw_version_to_gen(unsigned int version)
{
	uint8_t major = (version >> 24) & 0xff;

	switch (major) {
	case 1:
	case 2:	return SWITCHTEC_GEN3;
	case 3:
	case 4:
	case 5: return SWITCHTEC_GEN4;
	case 6:
	case 7:
	case 8: return SWITCHTEC_GEN5;
	case 9:
	case 10:
	case 11: return SWITCHTEC_GEN6;
	default: return SWITCHTEC_GEN_UNKNOWN;
	}
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_write_file)
		return GEN_OPS(dev)->fw_write_file(dev, fimg, dont_activate, force, progress_callback);
	errno = ENOTSUP;
	return -1;
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
	case SWITCHTEC_DLSTAT_PACKAGE_TOO_SMALL:
		msg = "Package length less than 32 bytes";  break;
	case SWITCHTEC_DLSTAT_SIG_MEM_ALLOC:
		msg = "Signature memory allocation failed";  break;
	case SWITCHTEC_DLSTAT_SEEPROM:
		msg = "SEEPROM download failed";  break;
	case SWITCHTEC_DLSTAT_READONLY_PARTITION:
		msg = "Programming a read-only partition";  break;
	case SWITCHTEC_DLSTAT_DOWNLOAD_TIMEOUT:
		msg = "Download Timeout";  break;
	case SWITCHTEC_DLSTAT_SEEPROM_TWI_NOT_ENABLED:
		msg = "SEEPROM or related TWI bus isn't enabled";  break;
	case SWITCHTEC_DLSTAT_PROGRAM_RUNNING:
		msg = "Programming a running partition";  break;
	case SWITCHTEC_DLSTAT_NOT_ALLOWED:
		msg = "Programming not allowed over this interface";  break;
	case SWITCHTEC_DLSTAT_XML_MISMATCH_ACT:
		msg = "Activation failed due to XML version mismatch";  break;
	case SWITCHTEC_DLSTAT_UNKNOWN_ACT:
		msg = "Activation failed due to unknown error";  break;
	case SWITCHTEC_DLSTAT_ERROR_OFFSET:
		msg = "Data offset error during programming";  break;
	case SWITCHTEC_DLSTAT_ERROR_PROGRAM:
		msg = "Failed to program to flash";  break;

	case SWITCHTEC_DLSTAT_NO_FILE:
		msg = "No Image Transferred"; break;
	default:
		fprintf(stderr, "%s: Unknown Error (0x%x)\n", s, ret);
		return;
	}

	fprintf(stderr, "%s: %s\n", s, msg);
}

struct metadata_hdr {
	char magic[4];
	char sub_magic[4];
	uint32_t hdr_version;
	uint32_t secure_version;
	uint32_t header_len;
	uint32_t metadata_len;
	uint32_t image_len;
	uint32_t type;
	uint8_t fw_id;
};

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

	if (!info)
		return 0;

	ret = read(fd, &magic, sizeof(magic));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(magic)) {
		errno = ENOEXEC;
		return -1;
	}

	struct metadata_hdr hdr;

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;
	
	if (!strncmp(hdr.magic, "MSCC", sizeof(hdr.magic)) && 
	    !strncmp(hdr.sub_magic, "_MD ", sizeof(hdr.sub_magic))) {
		if (hdr.fw_id)
			return switchtec_fw_file_info_gen5(fd, info);
		else
			return switchtec_fw_file_info_gen4(fd, info);
	} else if (!strncmp(hdr.magic, "PMC", sizeof(hdr.magic))) {
		return 1;
		//return switchtec_fw_file_info_gen3(fd, info);
	} else if (!strncmp(hdr.magic, "DCBI", sizeof(hdr.magic))) {
		return 1;	
	} else {
		goto invalid_file;
	}
	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
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

	if (!info.signed_image)
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
	case SWITCHTEC_FW_TYPE_RIOT:	return "RIOT";
	case SWITCHTEC_FW_TYPE_BL2:	return "BL2";
	case SWITCHTEC_FW_TYPE_NVLOG:	return "NVLOG";
	case SWITCHTEC_FW_TYPE_SEEPROM:	return "SEEPROM";
	default:			return "UNKNOWN";
	}
}

/**
 * @brief Return the firmware image information using supported metadata MRPC commands
 * for the gen6 BL2 phase.
 * @param[in]  dev	Switchtec device handle
 * @return pointer to switchtec_fw_image_info struct on success, NULL on failure
 */
struct switchtec_fw_image_info *switchtec_fw_part_data_bl2(struct switchtec_dev *dev)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_part_data_bl2)
		return GEN_OPS(dev)->fw_part_data_bl2(dev);
	errno = ENOTSUP;
	return NULL;
}

int switchtec_get_device_id_bl2(struct switchtec_dev *dev,
			        unsigned short *device_id)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->get_device_id_bl2)
		return GEN_OPS(dev)->get_device_id_bl2(dev, device_id);
	errno = ENOTSUP;
	return -1;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_part_summary)
		return GEN_OPS(dev)->fw_part_summary(dev);
	errno = ENOTSUP;
	return NULL;
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
 * @brief Free a firmware image info data structure
 * @param[in]  inf	The data structure to free.
 */
void switchtec_fw_image_info_free(struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_image_info *tmp;
	while (1)
	{
		if(inf->metadata)
			free(inf->metadata);

		if(inf->next) {
			tmp = inf;
			inf = inf->next;
			free(tmp);
		} else {
			return;
		}
	}
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_read)
		return GEN_OPS(dev)->fw_read(dev, addr, len, buf);
	errno = ENOTSUP;
	return -1;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_read_fd)
		return GEN_OPS(dev)->fw_read_fd(dev, fd, addr, len, progress_callback);
	errno = ENOTSUP;
	return -1;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_body_read_fd)
		return GEN_OPS(dev)->fw_body_read_fd(dev, fd, info, progress_callback);
	errno = ENOTSUP;
	return -1;
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
	errno = ENOTSUP;
	return -errno;
}

/**
 * @brief Check if the boot partition is marked as read-only
 * @param[in]  dev	Switchtec device handle
 * @return 1 if the partition is read-only, 0 if it's not or
 * 	a negative value if an error occurred
 */
int switchtec_fw_is_boot_ro(struct switchtec_dev *dev)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_is_boot_ro)
		return GEN_OPS(dev)->fw_is_boot_ro(dev);
	errno = ENOTSUP;
	return -1;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_set_boot_ro)
		return GEN_OPS(dev)->fw_set_boot_ro(dev, ro);
	errno = ENOTSUP;
	return -1;
}

/**@}*/