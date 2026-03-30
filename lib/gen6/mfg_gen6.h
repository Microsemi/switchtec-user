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

#ifndef LIBSWITCHTEC_MFG_GEN6_H
#define LIBSWITCHTEC_MFG_GEN6_H

#include "../switchtec_priv.h"

int switchtec_security_config_get_gen6(struct switchtec_dev *dev,
				       void *state);

int switchtec_security_config_set_gen6(struct switchtec_dev *dev,
				       void *setting);

int switchtec_mailbox_to_file_gen6(struct switchtec_dev *dev, int fd);

int switchtec_active_image_index_get_gen6(struct switchtec_dev *dev,
					  void *index);

int switchtec_active_image_index_set_gen6(struct switchtec_dev *dev,
					  void *index);

int switchtec_fw_exec_gen6(struct switchtec_dev *dev, int bl2);

int switchtec_boot_resume_gen6(struct switchtec_dev *dev);

int switchtec_sn_ver_get_gen6(struct switchtec_dev *dev, void *info);

int switchtec_secure_state_set_gen6(struct switchtec_dev *dev, int state);

int switchtec_kmsk_set_gen6(struct switchtec_dev *dev, void *public_key,
			    void *signature, void *kmsk);

int switchtec_debug_unlock_gen6(struct switchtec_dev *dev, uint32_t serial,
				uint32_t ver_sec_unlock, void *public_key,
				void *signature, void *token);

int switchtec_debug_lock_update_gen6(struct switchtec_dev *dev,
				     uint32_t serial,
				     uint32_t ver_sec_unlock,
				     void *public_key, void *signature);

int switchtec_security_settings_get_gen6(struct switchtec_dev *dev,
					 void *state);

int switchtec_dbg_unlock_get_token_gen6(struct switchtec_dev *dev, void *token,
					int token_type);

int switchtec_read_token_file_gen6(FILE *tkn_file, void *token);

#endif