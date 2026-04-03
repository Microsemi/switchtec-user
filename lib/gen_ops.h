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
#include "gen3/diag_gen3.h"

#include "gen6/mfg_gen6.h"
#include "gen5/mfg_gen5.h"
#include "gen4/mfg_gen4.h"

#include "gen6/fw_gen6.h"
#include "gen5/fw_gen5.h"
#include "gen4/fw_gen4.h"
#include "gen3/fw_gen3.h"

/**
 * @brief Gen6-specific operations vtable
 */
const struct switchtec_gen_ops switchtec_gen6_ops = {
	/* Diagnostics - NULL means use common implementation or not supported */
	.diag_cross_hair_enable = switchtec_diag_cross_hair_enable_gen4,
	.diag_cross_hair_disable = switchtec_diag_cross_hair_disable_gen4,
	.diag_cross_hair_get = switchtec_diag_cross_hair_get_gen4,
	.diag_eye_set_mode = NULL,
	.diag_eye_start = switchtec_diag_eye_start_gen6,
	.diag_eye_fetch = NULL,
	.diag_eye_cancel = NULL,
	.diag_eye_read = NULL,
	.diag_loopback_set = switchtec_diag_loopback_set_gen5,
	.diag_loopback_get = switchtec_diag_loopback_get_gen5,
	.diag_pattern_gen_set = switchtec_diag_pattern_gen_set_gen6,
	.diag_pattern_gen_get = switchtec_diag_pattern_gen_get_gen6,
	.diag_pattern_mon_set = switchtec_diag_pattern_mon_set_gen6,
	.diag_pattern_mon_get = switchtec_diag_pattern_mon_get_gen6,
	.diag_pattern_inject = switchtec_diag_pattern_inject_gen3,
	.diag_ltssm_log = switchtec_diag_ltssm_log_gen6,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = NULL,
	.diag_port_eq_tx_table = NULL,
	.diag_port_eq_tx_fslf = NULL,
	.diag_rcvr_obj = NULL,
	.diag_rcvr_ext = NULL,
	.aer_event_gen = NULL,
	.diag_perm_table = NULL,
	.diag_refclk_ctl = NULL,
	.diag_refclk_status = NULL,
	.inject_err_tlp_lcrc = switchtec_inject_err_tlp_lcrc_gen5,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = switchtec_inject_err_ack_nack_gen4,
	.inject_err_tlp_seqnum = switchtec_inject_err_tlp_seqnum_gen4,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = switchtec_inject_err_dllp_crc_gen4,
	.inject_err_dllp = NULL,
	.inject_err_dup_tlp = NULL,
	.osa_capture_data = switchtec_osa_capture_data_gen6,
	.osa_capture_control = switchtec_osa_capture_control_gen6,
	.osa_config_misc = switchtec_osa_config_misc_gen5,
	.osa_config_pattern = switchtec_osa_config_pattern_gen5,
	.osa_config_type = switchtec_osa_config_type_gen5,
	.osa_dump_conf = switchtec_osa_dump_conf_gen6,
	.osa = switchtec_osa_gen6,
	.aer_event_gen = NULL,

	/* Manufacturing */
	.security_config_get = switchtec_security_config_get_gen6,
	.security_config_set = switchtec_security_config_set_gen5,
	.mailbox_to_file = switchtec_mailbox_to_file_gen5,
	.active_image_index_get = switchtec_active_image_index_get_gen6,
	.active_image_index_set = switchtec_active_image_index_set_gen6,
	.fw_exec = switchtec_fw_exec_gen6,
	.boot_resume = switchtec_boot_resume_gen6,
	.sn_ver_get = switchtec_sn_ver_get_gen6,
	.secure_state_set = switchtec_secure_state_set_gen6,
	.kmsk_set = switchtec_kmsk_set_gen6,
	.debug_unlock = switchtec_debug_unlock_gen6,
	.debug_lock_update = switchtec_debug_lock_update_gen4,
	.security_settings_get = switchtec_security_settings_get_gen6,
	.debug_token_unlock_get_token = switchtec_dbg_unlock_get_token_gen6,
	.security_state_has_kmsk = switchtec_security_state_has_kmsk_gen4,
	.read_sec_cfg_file = switchtec_read_sec_cfg_file_gen5,
	.read_token_file = switchtec_read_token_file_gen6,
	.read_uds_file = switchtec_read_uds_file_gen4,
	.read_pubk_file = switchtec_read_pubk_file_gen4,
	.read_kmsk_file = switchtec_read_kmsk_file_gen4,
	.read_signature_file = switchtec_read_signature_file_gen4,

	/* Firmware */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = switchtec_fw_part_summary_gen6,
	.fw_img_write_hdr = switchtec_fw_img_write_hdr_gen6,
	.fw_file_info = switchtec_fw_file_info_gen5,
	.get_device_id_bl2 = switchtec_get_device_id_bl2_gen5,
	.fw_part_data_bl2 = switchtec_fw_part_data_bl2_gen6,
	.fw_set_redundant_flag = switchtec_fw_set_redundant_flag_gen6,
	.fw_toggle_active_partition = switchtec_fw_toggle_active_partition_gen6,
	.fw_img_get = switchtec_fw_img_get_gen6,
	.fw_write_file = switchtec_fw_write_file_gen4,
	.fw_read = switchtec_fw_read_gen4,
	.fw_read_fd = switchtec_fw_read_fd_gen4,
	.fw_body_read_fd = switchtec_fw_body_read_fd_gen4,
	.fw_is_boot_ro = NULL,
	.fw_set_boot_ro = NULL,
};

/**
 * @brief Gen5-specific operations vtable
 */
const struct switchtec_gen_ops switchtec_gen5_ops = {
	/* Diagnostics - NULL means use common implementation or not supported */
	.diag_cross_hair_enable = switchtec_diag_cross_hair_enable_gen4,
	.diag_cross_hair_disable = switchtec_diag_cross_hair_disable_gen4,
	.diag_cross_hair_get = switchtec_diag_cross_hair_get_gen4,
	.diag_eye_set_mode = switchtec_diag_eye_set_mode_gen3,
	.diag_eye_start = switchtec_diag_eye_start_gen5,
	.diag_eye_fetch = switchtec_diag_eye_fetch_gen3,
	.diag_eye_cancel = switchtec_diag_eye_cancel_gen3,
	.diag_eye_read = switchtec_diag_eye_read_gen5,
	.diag_loopback_set = switchtec_diag_loopback_set_gen5,
	.diag_loopback_get = switchtec_diag_loopback_get_gen5,
	.diag_pattern_gen_set = switchtec_diag_pattern_gen_set_gen5,
	.diag_pattern_gen_get = switchtec_diag_pattern_gen_get_gen3,
	.diag_pattern_mon_set = switchtec_diag_pattern_mon_set_gen3,
	.diag_pattern_mon_get = switchtec_diag_pattern_mon_get_gen3,
	.diag_pattern_inject = switchtec_diag_pattern_inject_gen3,
	.diag_ltssm_log = switchtec_diag_ltssm_log_gen5,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = switchtec_diag_port_eq_tx_coeff_gen5,
	.diag_port_eq_tx_table = switchtec_diag_port_eq_tx_table_gen5,
	.diag_port_eq_tx_fslf = switchtec_diag_port_eq_tx_fslf_gen5,
	.diag_rcvr_obj = NULL,
	.diag_rcvr_ext = NULL,
	.aer_event_gen = switchtec_aer_event_gen_gen3,
	.diag_perm_table = NULL,
	.diag_refclk_ctl = switchtec_diag_refclk_ctl_gen3,
	.diag_refclk_status = switchtec_diag_refclk_status_gen3,
	.inject_err_tlp_lcrc = switchtec_inject_err_tlp_lcrc_gen5,
	.inject_err_cto = switchtec_inject_err_cto_gen5,
	.inject_err_ack_nack = switchtec_inject_err_ack_nack_gen4,
	.inject_err_tlp_seqnum = switchtec_inject_err_tlp_seqnum_gen4,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = switchtec_inject_err_dllp_crc_gen4,
	.inject_err_dup_tlp = NULL,
	.osa_capture_data = switchtec_osa_capture_data_gen5,
	.osa_capture_control = switchtec_osa_capture_control_gen5,
	.osa_config_misc = switchtec_osa_config_misc_gen5,
	.osa_config_pattern = switchtec_osa_config_pattern_gen5,
	.osa_config_type = switchtec_osa_config_type_gen5,
	.osa_dump_conf = switchtec_osa_dump_conf_gen5,
	.osa = switchtec_osa_gen5,
	.aer_event_gen = switchtec_aer_event_gen_gen3,

	/* Manufacturing */
	.security_config_get = switchtec_security_config_get_gen5,
	.security_config_set = switchtec_security_config_set_gen5,
	.mailbox_to_file = switchtec_mailbox_to_file_gen5,
	.active_image_index_get = switchtec_active_image_index_get_gen5,
	.active_image_index_set = switchtec_active_image_index_set_gen5,
	.fw_exec = switchtec_fw_exec_gen5,
	.boot_resume = switchtec_boot_resume_gen5,
	.sn_ver_get = switchtec_sn_ver_get_gen5,
	.secure_state_set = switchtec_secure_state_set_gen5,
	.kmsk_set = switchtec_kmsk_set_gen5,
	.debug_unlock = switchtec_debug_unlock_gen4,
	.debug_lock_update = switchtec_debug_lock_update_gen4,
	.security_settings_get = NULL, //NOT Supported
	.debug_token_unlock_get_token = NULL, //NOT SUPPORTED
	.security_state_has_kmsk = switchtec_security_state_has_kmsk_gen4,
	.read_sec_cfg_file = switchtec_read_sec_cfg_file_gen5,
	.read_uds_file = switchtec_read_uds_file_gen4,
	.read_pubk_file = switchtec_read_pubk_file_gen4,
	.read_kmsk_file = switchtec_read_kmsk_file_gen4,
	.read_signature_file = switchtec_read_signature_file_gen4,

	/* Firmware */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = switchtec_fw_part_summary_gen5,
	.fw_img_write_hdr = switchtec_fw_img_write_hdr_gen5,
	.fw_file_info = switchtec_fw_file_info_gen5,
	.get_device_id_bl2 = switchtec_get_device_id_bl2_gen5,
	.fw_part_data_bl2 = NULL, // Not supported on Gen5
	.fw_set_redundant_flag = switchtec_fw_set_redundant_flag_gen5,
	.fw_toggle_active_partition = switchtec_fw_toggle_active_partition_gen5,
	.fw_img_get = NULL, // Not supported on Gen5
	.fw_write_file = switchtec_fw_write_file_gen4,
	.fw_read = switchtec_fw_read_gen4,
	.fw_read_fd = switchtec_fw_read_fd_gen4,
	.fw_body_read_fd = switchtec_fw_body_read_fd_gen4,
	.fw_is_boot_ro = NULL,
	.fw_set_boot_ro = NULL,
};

/**
 * @brief Gen4-specific operations vtable
 */
const struct switchtec_gen_ops switchtec_gen4_ops = {
	/* Diagnostics - NULL means use common implementation or not supported */
	.diag_cross_hair_enable = switchtec_diag_cross_hair_enable_gen4,
	.diag_cross_hair_disable = switchtec_diag_cross_hair_disable_gen4,
	.diag_cross_hair_get = switchtec_diag_cross_hair_get_gen4,
	.diag_eye_set_mode = switchtec_diag_eye_set_mode_gen3,
	.diag_eye_start = switchtec_diag_eye_start_gen3,
	.diag_eye_fetch = switchtec_diag_eye_fetch_gen3,
	.diag_eye_cancel = switchtec_diag_eye_cancel_gen3,
	.diag_eye_read = NULL,
	.diag_loopback_set = switchtec_diag_loopback_set_gen3,
	.diag_loopback_get = switchtec_diag_loopback_get_gen3,
	.diag_pattern_gen_set = switchtec_diag_pattern_gen_set_gen3,
	.diag_pattern_gen_get = switchtec_diag_pattern_gen_get_gen3,
	.diag_pattern_mon_set = switchtec_diag_pattern_mon_set_gen3,
	.diag_pattern_mon_get = switchtec_diag_pattern_mon_get_gen3,
	.diag_pattern_inject = switchtec_diag_pattern_inject_gen3,
	.diag_ltssm_log = switchtec_diag_ltssm_log_gen4,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = switchtec_diag_port_eq_tx_coeff_gen3,
	.diag_port_eq_tx_table = switchtec_diag_port_eq_tx_table_gen3,
	.diag_port_eq_tx_fslf = switchtec_diag_port_eq_tx_fslf_gen3,
	.diag_rcvr_obj = switchtec_diag_rcvr_obj_gen3,
	.diag_rcvr_ext = switchtec_diag_rcvr_ext_gen3,
	.aer_event_gen = switchtec_aer_event_gen_gen3,
	.diag_perm_table = switchtec_diag_perm_table_gen3,
	.diag_refclk_ctl = switchtec_diag_refclk_ctl_gen3,
	.diag_refclk_status = switchtec_diag_refclk_status_gen3,
	.inject_err_tlp_lcrc = switchtec_inject_err_tlp_lcrc_gen4,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = switchtec_inject_err_ack_nack_gen4,
	.inject_err_tlp_seqnum = switchtec_inject_err_tlp_seqnum_gen4,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = switchtec_inject_err_dllp_crc_gen4,
	.inject_err_dllp = switchtec_inject_err_dllp_gen4,
	.inject_err_dup_tlp = NULL,
	.osa_capture_data = NULL,
	.osa_capture_control = NULL,
	.osa_config_misc = NULL,
	.osa_config_pattern = NULL,
	.osa_config_type = NULL,
	.osa_dump_conf = NULL,
	.osa = NULL,

	/* Manufacturing */
	.security_config_get = switchtec_security_config_get_gen4,
	.security_config_set = switchtec_security_config_set_gen4,
	.mailbox_to_file = switchtec_mailbox_to_file_gen4,
	.active_image_index_get = switchtec_active_image_index_get_gen4,
	.active_image_index_set = switchtec_active_image_index_set_gen4,
	.fw_exec = switchtec_fw_exec_gen4,
	.boot_resume = switchtec_boot_resume_gen4,
	.sn_ver_get = switchtec_sn_ver_get_gen4,
	.secure_state_set = switchtec_secure_state_set_gen4,
	.kmsk_set = switchtec_kmsk_set_gen4,
	.debug_unlock = switchtec_debug_unlock_gen4,
	.debug_lock_update = switchtec_debug_lock_update_gen4,
	.security_settings_get = NULL, //Not supported
	.debug_token_unlock_get_token = NULL, //NOT SUPPORTED
	.security_state_has_kmsk = switchtec_security_state_has_kmsk_gen4,
	.read_uds_file = switchtec_read_uds_file_gen4,
	.read_sec_cfg_file = switchtec_read_sec_cfg_file_gen4,
	.read_pubk_file = switchtec_read_pubk_file_gen4,
	.read_kmsk_file = switchtec_read_kmsk_file_gen4,
	.read_signature_file = switchtec_read_signature_file_gen4,
	.dbg_unlock_version_update = switchtec_dbg_unlock_version_update_gen4,

	/* Firmware */
	.fw_part_id_to_type = NULL,
	.fw_type_to_part_id = NULL,
	.fw_part_id_to_str = NULL,
	.fw_part_summary = switchtec_fw_part_summary_gen4,
	.fw_img_write_hdr = switchtec_fw_img_write_hdr_gen4,
	.fw_file_info = switchtec_fw_file_info_gen4,
	.get_device_id_bl2 = switchtec_get_device_id_bl2_gen4,
	.fw_part_data_bl2 = NULL, // Not supported on Gen4
	.fw_set_redundant_flag = NULL, // Not supported on Gen4
	.fw_toggle_active_partition = switchtec_fw_toggle_active_partition_gen4,
	.fw_img_get = NULL, // Not supported on Gen4
	.fw_write_file = switchtec_fw_write_file_gen4,
	.fw_read = switchtec_fw_read_gen4,
	.fw_read_fd = switchtec_fw_read_fd_gen4,
	.fw_body_read_fd = switchtec_fw_body_read_fd_gen4,
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
	.diag_eye_set_mode = switchtec_diag_eye_set_mode_gen3,
	.diag_eye_start = switchtec_diag_eye_start_gen3,
	.diag_eye_fetch = switchtec_diag_eye_fetch_gen3,
	.diag_eye_cancel = switchtec_diag_eye_cancel_gen3,
	.diag_eye_read = NULL,
	.diag_loopback_set = switchtec_diag_loopback_set_gen3,
	.diag_loopback_get = switchtec_diag_loopback_get_gen3,
	.diag_pattern_gen_set = switchtec_diag_pattern_gen_set_gen3,
	.diag_pattern_gen_get = switchtec_diag_pattern_gen_get_gen3,
	.diag_pattern_mon_set = switchtec_diag_pattern_mon_set_gen3,
	.diag_pattern_mon_get = switchtec_diag_pattern_mon_get_gen3,
	.diag_pattern_inject = NULL,
	.diag_ltssm_log = NULL,
	.diag_ltssm_log_set = NULL,
	.diag_port_eq_tx_coeff = switchtec_diag_port_eq_tx_coeff_gen3,
	.diag_port_eq_tx_table = switchtec_diag_port_eq_tx_table_gen3,
	.diag_port_eq_tx_fslf = switchtec_diag_port_eq_tx_fslf_gen3,
	.diag_rcvr_obj = switchtec_diag_rcvr_obj_gen3,
	.diag_rcvr_ext = switchtec_diag_rcvr_ext_gen3,
	.aer_event_gen = switchtec_aer_event_gen_gen3,
	.diag_perm_table = switchtec_diag_perm_table_gen3,
	.diag_refclk_ctl = switchtec_diag_refclk_ctl_gen3,
	.diag_refclk_status = switchtec_diag_refclk_status_gen3,
	.inject_err_tlp_lcrc = NULL,
	.inject_err_cto = NULL,
	.inject_err_ack_nack = NULL,
	.inject_err_tlp_seqnum = NULL,
	.inject_err_tlp_ecrc = NULL,
	.inject_err_dllp_crc = NULL,
	.inject_err_dup_tlp = NULL,
	.osa_capture_data = NULL,
	.osa_capture_control = NULL,
	.osa_config_misc = NULL,
	.osa_config_pattern = NULL,
	.osa_config_type = NULL,
	.osa_dump_conf = NULL,
	.osa = NULL,

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
	.fw_part_summary = switchtec_fw_part_summary_gen3,
	.fw_file_info = switchtec_fw_file_info_gen3,
	.fw_img_write_hdr = switchtec_fw_img_write_hdr_gen3,
	.fw_read = switchtec_fw_read_gen3,
	.fw_is_boot_ro = switchtec_fw_is_boot_ro_gen3,
	.fw_set_boot_ro = switchtec_fw_set_boot_ro_gen3,
};

#endif