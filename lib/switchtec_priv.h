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

#ifndef LIBSWITCHTEC_SWITCHTEC_PRIV_H
#define LIBSWITCHTEC_SWITCHTEC_PRIV_H

#include "switchtec/switchtec.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

struct switchtec_dev;

/**
 * @brief The types of fw partitions
 */
enum switchtec_fw_image_part_id_gen3 {
	SWITCHTEC_FW_PART_ID_G3_BOOT = 0x0,
	SWITCHTEC_FW_PART_ID_G3_MAP0 = 0x1,
	SWITCHTEC_FW_PART_ID_G3_MAP1 = 0x2,
	SWITCHTEC_FW_PART_ID_G3_IMG0 = 0x3,
	SWITCHTEC_FW_PART_ID_G3_DAT0 = 0x4,
	SWITCHTEC_FW_PART_ID_G3_DAT1 = 0x5,
	SWITCHTEC_FW_PART_ID_G3_NVLOG = 0x6,
	SWITCHTEC_FW_PART_ID_G3_IMG1 = 0x7,
	SWITCHTEC_FW_PART_ID_G3_SEEPROM = 0xFE,
};

enum switchtec_fw_image_part_id_gen4 {
	SWITCHTEC_FW_PART_ID_G4_MAP0 = 0x0,
	SWITCHTEC_FW_PART_ID_G4_MAP1 = 0x1,
	SWITCHTEC_FW_PART_ID_G4_KEY0 = 0x2,
	SWITCHTEC_FW_PART_ID_G4_KEY1 = 0x3,
	SWITCHTEC_FW_PART_ID_G4_BL20 = 0x4,
	SWITCHTEC_FW_PART_ID_G4_BL21 = 0x5,
	SWITCHTEC_FW_PART_ID_G4_CFG0 = 0x6,
	SWITCHTEC_FW_PART_ID_G4_CFG1 = 0x7,
	SWITCHTEC_FW_PART_ID_G4_IMG0 = 0x8,
	SWITCHTEC_FW_PART_ID_G4_IMG1 = 0x9,
	SWITCHTEC_FW_PART_ID_G4_NVLOG = 0xa,
	SWITCHTEC_FW_PART_ID_G4_SEEPROM = 0xFE,
};


enum switchtec_fw_image_part_id_gen5 {
	SWITCHTEC_FW_PART_ID_G5_MAP0 = 0x0,
	SWITCHTEC_FW_PART_ID_G5_MAP1 = 0x1,
	SWITCHTEC_FW_PART_ID_G5_KEY0 = 0x2,
	SWITCHTEC_FW_PART_ID_G5_KEY1 = 0x3,
	SWITCHTEC_FW_PART_ID_G5_RIOT0 = 0x4,
	SWITCHTEC_FW_PART_ID_G5_RIOT1 = 0x5,
	SWITCHTEC_FW_PART_ID_G5_BL20 = 0x6,
	SWITCHTEC_FW_PART_ID_G5_BL21 = 0x7,
	SWITCHTEC_FW_PART_ID_G5_CFG0 = 0x8,
	SWITCHTEC_FW_PART_ID_G5_CFG1 = 0x9,
	SWITCHTEC_FW_PART_ID_G5_IMG0 = 0xa,
	SWITCHTEC_FW_PART_ID_G5_IMG1 = 0xb,
	SWITCHTEC_FW_PART_ID_G5_NVLOG = 0xc,
	SWITCHTEC_FW_PART_ID_G5_SEEPROM = 0xFE,
};

enum switchtec_fw_image_part_id_gen6 {
	SWITCHTEC_FW_PART_ID_G6_MAP0 = 0x0,
	SWITCHTEC_FW_PART_ID_G6_MAP1 = 0x1,
	SWITCHTEC_FW_PART_ID_G6_KEY0 = 0x2,
	SWITCHTEC_FW_PART_ID_G6_KEY1 = 0x3,
	SWITCHTEC_FW_PART_ID_G6_BL20 = 0x4,
	SWITCHTEC_FW_PART_ID_G6_BL21 = 0x5,
	SWITCHTEC_FW_PART_ID_G6_CFG0 = 0x6,
	SWITCHTEC_FW_PART_ID_G6_CFG1 = 0x7,
	SWITCHTEC_FW_PART_ID_G6_IMG0 = 0x8,
	SWITCHTEC_FW_PART_ID_G6_IMG1 = 0x9,
	SWITCHTEC_FW_PART_ID_G6_NVLOG = 0xa,
	SWITCHTEC_FW_PART_ID_G6_SEEPROM = 0xFE,
};

enum switchtec_ops_flags {
	SWITCHTEC_OPS_FLAG_NO_MFG = (1 << 0),
};

/**
 * @brief Generation-specific operations vtable
 *
 * This structure contains function pointers for operations that
 * differ between switch generations (Gen4, Gen5, Gen6).
 */
struct switchtec_gen_ops {
	/* Diagnostics */
	int (*diag_cross_hair_enable)(struct switchtec_dev *dev, int lane_id);
	int (*diag_cross_hair_disable)(struct switchtec_dev *dev);
	int (*diag_cross_hair_get)(struct switchtec_dev *dev, int start_lane_id,
				   int num_lanes, void *res);

	int (*diag_eye_set_mode)(struct switchtec_dev *dev, int mode);
	int (*diag_eye_start)(struct switchtec_dev *dev, int lane_mask[4],
			      void *x_range, void *y_range,
			      int step_interval, int capture_depth, int sar_sel,
			      int intleav_sel, int hstep, int data_mode,
			      int eye_mode, uint64_t refclk, int vstep);
	int (*diag_eye_fetch)(struct switchtec_dev *dev, double *pixels,
			      size_t pixel_cnt, int *lane_id);
	int (*diag_eye_cancel)(struct switchtec_dev *dev);
	int (*diag_eye_read)(struct switchtec_dev *dev, int lane_id,
		      	    int bin, int* num_phases, double* ber_data);

	int (*diag_loopback_set)(struct switchtec_dev *dev, int port_id,
				 int enable, int enable_parallel,
				 int enable_external, int enable_ltssm,
				 int enable_pipe, int ltssm_speed);
	int (*diag_loopback_get)(struct switchtec_dev *dev, int port_id,
				 int *enabled, int *ltssm_speed);

	int (*diag_pattern_gen_set)(struct switchtec_dev *dev, int port_id,
				    int type, int link_speed);
	int (*diag_pattern_gen_get)(struct switchtec_dev *dev, int port_id,
				    int *type);
	int (*diag_pattern_mon_set)(struct switchtec_dev *dev, int port_id,
				    int type);
	int (*diag_pattern_mon_get)(struct switchtec_dev *dev, int port_id,
				    int lane_id, int *type,
				    unsigned long long *err_cnt);
	int (*diag_pattern_inject)(struct switchtec_dev *dev, int port_id,
				   int err_cnt);

	int (*diag_ltssm_log)(struct switchtec_dev *dev, int port,
			      int *log_count, void *log_data);
	int (*diag_ltssm_log_set)(struct switchtec_dev *dev, int port, int mode,
				  int trigger_link_rate);

	int (*diag_port_eq_tx_coeff)(struct switchtec_dev *dev, int port_id,
				     int prev_speed, int end, int link,
				     void *res);
	int (*diag_port_eq_tx_table)(struct switchtec_dev *dev, int port_id,
				     int prev_speed, int link, void *res);
	int (*diag_port_eq_tx_fslf)(struct switchtec_dev *dev, int port_id,
				    int prev_speed, int lane_id, int end,
				    int link, void *res);

	int (*diag_rcvr_obj)(struct switchtec_dev *dev, int port_id, int lane_id,
			     int link, void *res);
	int (*diag_rcvr_ext)(struct switchtec_dev *dev, int port_id, int lane_id,
			     int link, void *res);

	int (*diag_refclk_ctl)(struct switchtec_dev *dev, int stack_id,
			       int enable);

	int (*inject_err_tlp_lcrc)(struct switchtec_dev *dev, int phys_port,
				   int enable, uint8_t rate);
	int (*inject_err_tlp_seqnum)(struct switchtec_dev *dev, int phys_port);
	int (*inject_err_tlp_ecrc)(struct switchtec_dev *dev, int phys_port,
				   int enable, uint8_t rate);
	int (*inject_err_dllp_crc)(struct switchtec_dev *dev, int phys_port,
				   int enable, uint16_t rate);
	int (*inject_err_dllp)(struct switchtec_dev *dev, int phys_port_id,
				int data);
	int (*inject_err_dup_tlp)(struct switchtec_dev *dev, int phys_port,
				  int enable, uint8_t rate);
	int (*inject_err_cto)(struct switchtec_dev *dev, int phys_port_id);
	int (*inject_err_ack_nack)(struct switchtec_dev *dev, int phys_port_id,
					uint16_t seq_num, uint8_t count);

	/* Manufacturing */
	int (*security_config_get)(struct switchtec_dev *dev, void *state);
	int (*security_config_set)(struct switchtec_dev *dev, void *setting);
	int (*mailbox_to_file)(struct switchtec_dev *dev, int fd);
	int (*active_image_index_get)(struct switchtec_dev *dev, void *index);
	int (*active_image_index_set)(struct switchtec_dev *dev, void *index);
	int (*fw_exec)(struct switchtec_dev *dev, int bl2);
	int (*boot_resume)(struct switchtec_dev *dev);
	int (*sn_ver_get)(struct switchtec_dev *dev, void *info);
	int (*secure_state_set)(struct switchtec_dev *dev, int state);
	int (*kmsk_set)(struct switchtec_dev *dev, void *public_key,
			void *signature, void *kmsk);
	int (*debug_unlock)(struct switchtec_dev *dev, uint32_t serial,
			    uint32_t ver_sec_unlock, void *public_key,
			    void *signature, void *token);
	int (*debug_lock_update)(struct switchtec_dev *dev, uint32_t serial,
				 uint32_t ver_sec_unlock, void *public_key,
				 void *signature);
	int (*security_settings_get)(struct switchtec_dev *dev, void *state);
	int (*debug_token_unlock_get_token)(struct switchtec_dev *dev, void *token, int token_type);
	int (*security_state_has_kmsk)(void *state, void *kmsk);
	int (*read_uds_file)(FILE *uds_file, void *uds);
	int (*read_sec_cfg_file)(struct switchtec_dev *dev, FILE *setting_file,
				 void *set);
	int (*read_pubk_file)(FILE *pubk_file, void *pubk);
	int (*read_kmsk_file)(FILE *kmsk_file, void *kmsk);
	int (*read_signature_file)(FILE *sig_file, void *signature);
	int (*read_token_file)(FILE *tkn_file, void *token);
	int (*dbg_unlock_version_update)(struct switchtec_dev *dev,
					 uint32_t serial,
					 uint32_t ver_sec_unlock,
					 void *public_key,
					 void *signature);
	/* Firmware */
	int (*fw_part_id_to_type)(int part_id);
	int (*fw_type_to_part_id)(int type);
	const char *(*fw_part_id_to_str)(int part_id);
	int (*fw_img_write_hdr)(int fd, struct switchtec_fw_image_info *info);
	struct switchtec_fw_part_summary *(*fw_part_summary)(struct switchtec_dev *dev);
	int (*fw_file_info)(int fd, struct switchtec_fw_image_info *info);
	int (*get_device_id_bl2)(struct switchtec_dev *dev,
				 unsigned short *device_id);
	struct switchtec_fw_image_info *
	(*fw_part_data_bl2)(struct switchtec_dev *dev);
	int (*fw_set_redundant_flag)(struct switchtec_dev *dev, int keyman,
				     int riot, int bl2, int cfg, int fw,
				     int set);
	int (*fw_toggle_active_partition)(struct switchtec_dev *dev,
					  int toggle_bl2, int toggle_key,
					  int toggle_fw, int toggle_cfg,
					  int toggle_riotcore);
	int (*fw_img_get)(struct switchtec_dev *dev, int fd,
			  enum switchtec_fw_type_gen6 fw_type, int fw_slot,
			  void (*progress_callback)(int cur, int tot));
	int (*fw_write_file)(struct switchtec_dev *dev, FILE *fimg,
			    int dont_activate, int force,
			    void (*progress_callback)(int cur, int tot));
	int (*fw_read)(struct switchtec_dev *dev, unsigned long addr, size_t len, void *buf);
	int (*fw_read_fd)(struct switchtec_dev *dev, int fd, unsigned long addr, 
			      size_t len, void (*progress_callback)(int cur, int tot));
	int (*fw_body_read_fd)(struct switchtec_dev *dev, int fd,
			      struct switchtec_fw_image_info *info,
			      void (*progress_callback)(int cur, int tot));
	int (*fw_is_boot_ro) (struct switchtec_dev *dev);
	int (*fw_set_boot_ro) (struct switchtec_dev *dev, enum switchtec_fw_ro ro);
};

/* Generation ops table indexed by switchtec_gen enum */
extern const struct switchtec_gen_ops *switchtec_gen_ops[];

/* Helper macro to call generation-specific operations */
#define GEN_OPS(dev) (switchtec_gen_ops[switchtec_gen(dev)])

struct switchtec_ops {
	int flags;

	void (*close)(struct switchtec_dev *dev);
	int (*get_device_id)(struct switchtec_dev *dev);
	int (*get_fw_version)(struct switchtec_dev *dev, char *buf,
			      size_t buflen);
	int (*get_device_version)(struct switchtec_dev *dev, int *device_version);
	int (*cmd)(struct switchtec_dev *dev,  uint32_t cmd,
		   const void *payload, size_t payload_len, void *resp,
		   size_t resp_len);
	int (*get_devices)(struct switchtec_dev *dev,
			   struct switchtec_status *status,
			   int ports);
	int (*pff_to_port)(struct switchtec_dev *dev, int pff,
			   int *partition, int *port);
	int (*port_to_pff)(struct switchtec_dev *dev, int partition,
			   int port, int *pff);
	gasptr_t (*gas_map)(struct switchtec_dev *dev, int writeable,
			    size_t *map_size);
	void (*gas_unmap)(struct switchtec_dev *dev, gasptr_t map);
	int (*flash_part)(struct switchtec_dev *dev,
			  struct switchtec_fw_image_info *info,
			  enum switchtec_fw_image_part_id_gen3 part);
	int (*event_summary)(struct switchtec_dev *dev,
			     struct switchtec_event_summary *sum);
	int (*event_ctl)(struct switchtec_dev *dev,
			 enum switchtec_event_id e,
			 int index, int flags,
			 uint32_t data[5]);
	int (*event_wait)(struct switchtec_dev *dev, int timeout_ms);
	int (*event_wait_for)(struct switchtec_dev *dev,
			      enum switchtec_event_id e, int index,
			      struct switchtec_event_summary *res,
			      int timeout_ms);

	uint8_t (*gas_read8)(struct switchtec_dev *dev, uint8_t __gas *addr);
	uint16_t (*gas_read16)(struct switchtec_dev *dev, uint16_t __gas *addr);
	uint32_t (*gas_read32)(struct switchtec_dev *dev, uint32_t __gas *addr);
	uint64_t (*gas_read64)(struct switchtec_dev *dev, uint64_t __gas *addr);

	void (*gas_write8)(struct switchtec_dev *dev, uint8_t val,
			   uint8_t __gas *addr);
	void (*gas_write16)(struct switchtec_dev *dev, uint16_t val,
			    uint16_t __gas *addr);
	void (*gas_write32)(struct switchtec_dev *dev, uint32_t val,
			    uint32_t __gas *addr);
	void (*gas_write32_no_retry)(struct switchtec_dev *dev, uint32_t val,
				     uint32_t __gas *addr);
	void (*gas_write64)(struct switchtec_dev *dev, uint64_t val,
			    uint64_t __gas *addr);

	void (*memcpy_to_gas)(struct switchtec_dev *dev, void __gas *dest,
			      const void *src, size_t n);
	void (*memcpy_from_gas)(struct switchtec_dev *dev, void *dest,
				const void __gas *src, size_t n);
	ssize_t (*write_from_gas)(struct switchtec_dev *dev, int fd,
				  const void __gas *src, size_t n);
};

int switchtec_flash_part(struct switchtec_dev *dev,
			 struct switchtec_fw_image_info *info,
			 enum switchtec_fw_image_part_id_gen3 part);

struct switchtec_dev {
	int device_id;
	enum switchtec_gen gen;
	enum switchtec_variant var;
	int pax_id;
	int local_pax_id;
	int partition, partition_count;
	enum switchtec_boot_phase boot_phase;
	char name[PATH_MAX];

	gasptr_t gas_map;
	size_t gas_map_size;

	const struct switchtec_ops *ops;
};

extern const struct switchtec_mrpc switchtec_mrpc_table[MRPC_MAX_ID];

static inline void version_to_string(uint32_t version, char *buf, size_t buflen)
{
	int major = version >> 24;
	int minor = (version >> 16) & 0xFF;
	int build = version & 0xFFFF;

	snprintf(buf, buflen, "%x.%02x B%03X", major, minor, build);
}

const char *platform_strerror();

static inline uint8_t __gas_read8(struct switchtec_dev *dev,
				  uint8_t __gas *addr)
{
	return dev->ops->gas_read8(dev, addr);
}

static inline uint16_t __gas_read16(struct switchtec_dev *dev,
				    uint16_t __gas *addr)
{
	return dev->ops->gas_read16(dev, addr);
}

static inline uint32_t __gas_read32(struct switchtec_dev *dev,
				    uint32_t __gas *addr)
{
	return dev->ops->gas_read32(dev, addr);
}

static inline uint64_t __gas_read64(struct switchtec_dev *dev,
				    uint64_t __gas *addr)
{
	return dev->ops->gas_read64(dev, addr);
}

static inline void __gas_write8(struct switchtec_dev *dev, uint8_t val,
				uint8_t __gas *addr)
{
	dev->ops->gas_write8(dev, val, addr);
}

static inline void __gas_write16(struct switchtec_dev *dev, uint16_t val,
				 uint16_t __gas *addr)
{
	dev->ops->gas_write16(dev, val, addr);
}

static inline void __gas_write32(struct switchtec_dev *dev, uint32_t val,
				 uint32_t __gas *addr)
{
	dev->ops->gas_write32(dev, val, addr);
}

static inline void __gas_write32_no_retry(struct switchtec_dev *dev,
					  uint32_t val,
					  uint32_t __gas *addr)
{
	dev->ops->gas_write32_no_retry(dev, val, addr);
}

static inline void __gas_write64(struct switchtec_dev *dev, uint64_t val,
				 uint64_t __gas *addr)
{
	dev->ops->gas_write64(dev, val, addr);
}

static inline void __memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
		   const void *src, size_t n)
{
	dev->ops->memcpy_to_gas(dev, dest, src, n);
}

static inline void __memcpy_from_gas(struct switchtec_dev *dev, void *dest,
		     const void __gas *src, size_t n)
{
	dev->ops->memcpy_from_gas(dev, dest, src, n);
}

static inline ssize_t __write_from_gas(struct switchtec_dev *dev, int fd,
		       const void __gas *src, size_t n)
{
	return dev->ops->write_from_gas(dev, fd, src, n);
}

#endif
