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

#ifndef LIBSWITCHTEC_GEN_OPS_H
#define LIBSWITCHTEC_GEN_OPS_H

#include "switchtec_priv.h"
#include "gen6/diag_gen6.h"
#include "gen5/diag_gen5.h"
#include "gen4/diag_gen4.h"
#include "gen6/mfg_gen6.h"
#include "gen5/mfg_gen5.h"
#include "gen4/mfg_gen4.h"
#include "gen4/fw_gen4.h"
#include "gen5/fw_gen5.h"
#include "gen6/fw_gen6.h"

/**
 * @brief Gen6-specific operations vtable
 */
const struct switchtec_gen_ops switchtec_gen6_ops = {
	/* Diagnostics - NULL means use common implementation or not supported */
	.diag_cross_hair_enable = NULL,
	.diag_cross_hair_disable = NULL,
	.diag_cross_hair_get = NULL,
	.diag_eye_set_mode = NULL,
	.diag_eye_start = NULL,
	.diag_eye_fetch = NULL,
	.diag_eye_cancel = NULL,
	.diag_eye_read = NULL,
	.diag_loopback_set = NULL,
	.diag_loopback_get = NULL,
	.diag_pattern_gen_set = NULL,
	.diag_pattern_gen_get = NULL,
	.diag_pattern_mon_set = NULL,
	.diag_pattern_mon_get = NULL,
	.diag_pattern_inject = NULL,
	.diag_ltssm_log = NULL,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = NULL,
	.diag_port_eq_tx_table = NULL,
	.diag_port_eq_tx_fslf = NULL,
	.diag_rcvr_obj = NULL,
	.diag_rcvr_ext = NULL,
	.diag_refclk_ctl = NULL,
	.inject_err_tlp_lcrc = NULL,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = NULL,
	.inject_err_tlp_seqnum = NULL,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = NULL,
	.inject_err_dllp = NULL,
	.inject_err_dup_tlp = NULL,

	/* Manufacturing */
	.security_config_get = NULL,
	.security_config_set = NULL,
	.mailbox_to_file = NULL,
	.active_image_index_get = NULL,
	.active_image_index_set = NULL,
	.fw_exec = NULL,
	.boot_resume = NULL,
	.sn_ver_get = NULL,
	.secure_state_set = NULL,
	.kmsk_set = NULL,
	.debug_unlock = NULL,
	.debug_lock_update = NULL,
	.security_settings_get = NULL,
	.debug_token_unlock_get_token = NULL,
	.security_state_has_kmsk = NULL,
	.read_sec_cfg_file = NULL,
	.read_token_file = NULL,
	.read_uds_file = NULL,
	.read_pubk_file = NULL,
	.read_kmsk_file = NULL,
	.read_signature_file = NULL,

	/* Firmware */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = NULL,
	.fw_img_write_hdr = NULL,
	.fw_file_info = NULL,
	.get_device_id_bl2 = NULL,
	.fw_part_data_bl2 = NULL,
	.fw_set_redundant_flag = NULL,
	.fw_toggle_active_partition = NULL,
	.fw_img_get = NULL,
	.fw_write_file = NULL,
	.fw_read = NULL,
	.fw_read_fd = NULL,
	.fw_body_read_fd = NULL,
	.fw_is_boot_ro = NULL,
	.fw_set_boot_ro = NULL,
};

/**
 * @brief Gen5-specific operations vtable
 */
const struct switchtec_gen_ops switchtec_gen5_ops = {
	/* Diagnostics - NULL means use common implementation or not supported */
	.diag_cross_hair_enable = NULL,
	.diag_cross_hair_disable = NULL,
	.diag_cross_hair_get = NULL,
	.diag_eye_set_mode = NULL,
	.diag_eye_start = NULL,
	.diag_eye_fetch = NULL,
	.diag_eye_cancel = NULL,
	.diag_eye_read = NULL,
	.diag_loopback_set = NULL,
	.diag_loopback_get = NULL,
	.diag_pattern_gen_set = NULL,
	.diag_pattern_gen_get = NULL,
	.diag_pattern_mon_set = NULL,
	.diag_pattern_mon_get = NULL,
	.diag_pattern_inject = NULL,
	.diag_ltssm_log = NULL,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = NULL,
	.diag_port_eq_tx_table = NULL,
	.diag_port_eq_tx_fslf = NULL,
	.diag_rcvr_obj = NULL,
	.diag_rcvr_ext = NULL,
	.diag_refclk_ctl = NULL,
	.inject_err_tlp_lcrc = NULL,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = NULL,
	.inject_err_tlp_seqnum = NULL,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = NULL,
	.inject_err_dup_tlp = NULL,

	/* Manufacturing */
	.security_config_get = NULL,
	.security_config_set = NULL,
	.mailbox_to_file = NULL,
	.active_image_index_get = NULL,
	.active_image_index_set = NULL,
	.fw_exec = NULL,
	.boot_resume = NULL,
	.sn_ver_get = NULL,
	.secure_state_set = NULL,
	.kmsk_set = NULL,
	.debug_unlock = NULL,
	.debug_lock_update = NULL,
	.security_settings_get = NULL, //NOT Supported
	.debug_token_unlock_get_token = NULL, //NOT SUPPORTED
	.security_state_has_kmsk = NULL,
	.read_sec_cfg_file = NULL,
	.read_uds_file = NULL,
	.read_pubk_file = NULL,
	.read_kmsk_file = NULL,
	.read_signature_file = NULL,

	/* Firmware */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = NULL,
	.fw_img_write_hdr = NULL,
	.fw_file_info = NULL,
	.get_device_id_bl2 = NULL,
	.fw_part_data_bl2 = NULL, // Not supported on Gen5
	.fw_set_redundant_flag = NULL,
	.fw_toggle_active_partition = NULL,
	.fw_img_get = NULL, // Not supported on Gen5
	.fw_write_file = NULL,
	.fw_read = NULL,
	.fw_read_fd = NULL,
	.fw_body_read_fd = NULL,
	.fw_is_boot_ro = NULL,
	.fw_set_boot_ro = NULL,
};

/**
 * @brief Gen4-specific operations vtable
 */
const struct switchtec_gen_ops switchtec_gen4_ops = {
	/* Diagnostics - NULL means use common implementation or not supported */
	.diag_cross_hair_enable = NULL,
	.diag_cross_hair_disable = NULL,
	.diag_cross_hair_get = NULL,
	.diag_eye_set_mode = NULL,
	.diag_eye_start = NULL,
	.diag_eye_fetch = NULL,
	.diag_eye_cancel = NULL,
	.diag_eye_read = NULL,
	.diag_loopback_set = NULL,
	.diag_loopback_get = NULL,
	.diag_pattern_gen_set = NULL,
	.diag_pattern_gen_get = NULL,
	.diag_pattern_mon_set = NULL,
	.diag_pattern_mon_get = NULL,
	.diag_pattern_inject = NULL,
	.diag_ltssm_log = NULL,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = NULL,
	.diag_port_eq_tx_table = NULL,
	.diag_port_eq_tx_fslf = NULL,
	.diag_rcvr_obj = NULL,
	.diag_rcvr_ext = NULL,
	.diag_refclk_ctl = NULL,
	.inject_err_tlp_lcrc = NULL,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = NULL,
	.inject_err_tlp_seqnum = NULL,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = NULL,
	.inject_err_dllp = NULL,
	.inject_err_dup_tlp = NULL,

	/* Manufacturing */
	.security_config_get = NULL,
	.security_config_set = NULL,
	.mailbox_to_file = NULL,
	.active_image_index_get = NULL,
	.active_image_index_set = NULL,
	.fw_exec = NULL,
	.boot_resume = NULL,
	.sn_ver_get = NULL,
	.secure_state_set = NULL,
	.kmsk_set = NULL,
	.debug_unlock = NULL,
	.debug_lock_update = NULL,
	.security_settings_get = NULL, //Not supported
	.debug_token_unlock_get_token = NULL, //NOT SUPPORTED
	.security_state_has_kmsk = NULL,
	.read_uds_file = NULL,
	.read_sec_cfg_file = NULL,
	.read_pubk_file = NULL,
	.read_kmsk_file = NULL,
	.read_signature_file = NULL,
	.dbg_unlock_version_update = NULL,

	/* Firmware */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = NULL,
	.fw_img_write_hdr = NULL,
	.fw_file_info = NULL,
	.get_device_id_bl2 = NULL,
	.fw_part_data_bl2 = NULL, // Not supported on Gen4
	.fw_set_redundant_flag = NULL, // Not supported on Gen4
	.fw_toggle_active_partition = NULL,
	.fw_img_get = NULL, // Not supported on Gen4
	.fw_write_file = NULL,
	.fw_read = NULL,
	.fw_read_fd = NULL,
	.fw_body_read_fd = NULL,
	.fw_is_boot_ro = NULL,
	.fw_set_boot_ro = NULL,
};

/**
 * @brief Gen3-specific operations vtable
 *
 * Gen3 is legacy and has different partition structures.
 * Most operations are not supported or use different implementations.
 */
const struct switchtec_gen_ops switchtec_gen3_ops = {
	/* Diagnostics - NULL means not supported on Gen3 */
	.diag_cross_hair_enable = NULL,
	.diag_cross_hair_disable = NULL,
	.diag_cross_hair_get = NULL,
	.diag_eye_set_mode = NULL,
	.diag_eye_start = NULL,
	.diag_eye_fetch = NULL,
	.diag_eye_cancel = NULL,
	.diag_eye_read = NULL,
	.diag_loopback_set = NULL,
	.diag_loopback_get = NULL,
	.diag_pattern_gen_set = NULL,
	.diag_pattern_gen_get = NULL,
	.diag_pattern_mon_set = NULL,
	.diag_pattern_mon_get = NULL,
	.diag_pattern_inject = NULL,
	.diag_ltssm_log = NULL, //Not supported
	.diag_ltssm_log_set = NULL, //Not supported
	.diag_port_eq_tx_coeff = NULL,
	.diag_port_eq_tx_table = NULL,
	.diag_port_eq_tx_fslf = NULL,
	.diag_rcvr_obj = NULL,
	.diag_rcvr_ext = NULL,
	.diag_refclk_ctl = NULL,
	.inject_err_tlp_lcrc = NULL,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = NULL,
	.inject_err_tlp_seqnum = NULL,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = NULL,
	.inject_err_dup_tlp = NULL,

	/* Manufacturing - Gen3 uses different structures */
	.security_config_get = NULL,
	.security_config_set = NULL,
	.mailbox_to_file = NULL,
	.active_image_index_get = NULL,
	.active_image_index_set = NULL,
	.fw_exec = NULL,
	.boot_resume = NULL,
	.sn_ver_get = NULL,
	.secure_state_set = NULL,
	.kmsk_set = NULL,
	.debug_unlock = NULL,
	.debug_lock_update = NULL,
	.security_settings_get = NULL,
	.debug_token_unlock_get_token = NULL,
	.security_state_has_kmsk = NULL,

	/* Firmware - Gen3 uses different partition scheme */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = NULL,
	.fw_file_info = NULL,
	.fw_img_write_hdr = NULL,
	//.fw_read = NULL, add later implement gen3 like other gens
	.fw_is_boot_ro = NULL,
	.fw_set_boot_ro = NULL,
};

#endif