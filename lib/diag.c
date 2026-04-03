/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2021, Microsemi Corporation
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
 * @brief Switchtec diagnostic functions
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec_priv.h"
#include "switchtec/diag.h"
#include "switchtec/endian.h"
#include "switchtec/switchtec.h"
#include "switchtec/utils.h"

#include <errno.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/**
 * @brief Enable cross hair on specified lane
 * @param[in]  dev	Switchtec device handle
 * @param[in]  lane_id	Lane to enable, or SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES
 *			for all lanes.
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_cross_hair_enable(struct switchtec_dev *dev, int lane_id)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_cross_hair_enable)
		return GEN_OPS(dev)->diag_cross_hair_enable(dev, lane_id);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Disable active cross hair
 * @param[in]  dev	Switchtec device handle
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_cross_hair_disable(struct switchtec_dev *dev)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_cross_hair_disable)
		return GEN_OPS(dev)->diag_cross_hair_disable(dev);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get cross hair data
 * @param[in]  dev		Switchtec device handle
 * @param[in]  start_lane_id	Start lane ID to get
 * @param[in]  num_lanes	Number of lanes to get
 * @param[out] res		Resulting cross hair data
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_cross_hair_get(struct switchtec_dev *dev, int start_lane_id,
		int num_lanes, struct switchtec_diag_cross_hair *res)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_cross_hair_get)
		return GEN_OPS(dev)->diag_cross_hair_get(dev, start_lane_id,
							 num_lanes, res);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Set the data mode for the next Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  mode	       Mode to use (raw or ratio)
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_set_mode(struct switchtec_dev *dev,
				enum switchtec_diag_eye_data_mode mode)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_eye_set_mode)
		return GEN_OPS(dev)->diag_eye_set_mode(dev, mode);
	errno = ENOTSUP;
	return -1;	
}

/**
 * @brief Start a PCIe Eye Read Gen5
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  lane_id         lane_id
 * @param[in]  bin             bin
 * @param[in]  num_phases      pointer to the number of phases
 * @param[in]  ber_data        pointer to the Ber data
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_read(struct switchtec_dev *dev, int lane_id,
		      	    int bin, int* num_phases, double* ber_data)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_eye_read)
		return GEN_OPS(dev)->diag_eye_read(dev, lane_id, bin, num_phases, ber_data);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Start a PCIe Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  lane_mask       Bitmap of the lanes to capture
 * @param[in]  x_range         Time range: start should be between 0 and 63,
 *			       end between start and 63.
 * @param[in]  y_range         Voltage range: start should be between -255 and 255,
 *			       end between start and 255.
 * @param[in]  step_interval   Sampling time in milliseconds for each step
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_start(struct switchtec_dev *dev, int lane_mask[4],
			     struct range *x_range, struct range *y_range,
			     int step_interval, int capture_depth, int sar_sel,
			     int intleav_sel, int hstep, int data_mode, 
			     int eye_mode, uint64_t refclk, int vstep)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_eye_start)
		return GEN_OPS(dev)->diag_eye_start(dev, lane_mask, x_range, 
						y_range, step_interval, 
						capture_depth, sar_sel, 
						intleav_sel, hstep, data_mode, 
						eye_mode, refclk, vstep);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Start a PCIe Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[out] pixels          Resulting pixel data
 * @param[in]  pixel_cnt       Space in pixel array
 * @param[out] lane_id         The lane for the resulting pixels
 *
 * @return number of pixels fetched on success, error code on failure
 *
 * pixel_cnt needs to be greater than 62 in raw mode or 496 in ratio
 * mode, otherwise data will be lost and the number of pixels fetched
 * will be greater than the space in the pixel buffer.
 */
int switchtec_diag_eye_fetch(struct switchtec_dev *dev, double *pixels,
			     size_t pixel_cnt, int *lane_id)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_eye_fetch)
		return GEN_OPS(dev)->diag_eye_fetch(dev, pixels, pixel_cnt, lane_id);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Cancel in-progress eye capture
 * @param[in]  dev	       Switchtec device handle
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_cancel(struct switchtec_dev *dev)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_eye_cancel)
		return GEN_OPS(dev)->diag_eye_cancel(dev);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Setup Loopback Mode
 * @param[in]  dev	    Switchtec device handle
 * @param[in]  port_id	    Physical port ID
 * @param[in]  enable		Enable bitmap - Gen 4
 * @param[in]  enable_parallel	Enable the parallel SERDES loopback - Gen 5
 * @param[in]  enable_external	Enable the external physical loopback - Gen 5
 * @param[in]  enable_ltssm	Enable the ltssm loopback
 * @param[in]  ltssm_speed  LTSSM loopback max speed
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_loopback_set(struct switchtec_dev *dev, int port_id,
				int enable, int enable_parallel,
				int enable_external, int enable_ltssm,
				int enable_pipe,
				enum switchtec_diag_ltssm_speed ltssm_speed)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_loopback_set)
		return GEN_OPS(dev)->diag_loopback_set(dev, port_id, enable,
						       enable_parallel,
						       enable_external,
						       enable_ltssm,
						       enable_pipe, ltssm_speed);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get Loopback Mode
 * @param[in]  dev	     Switchtec device handle
 * @param[in]  port_id	     Physical port ID
 * @param[out] enabled       Set of enum switchtec_diag_loopback_enable
 *			     indicating which loopback modes are enabled
 * @param[out] ltssm_speed   LTSSM loopback max speed
 *
 * @return 0 on succes, error code on failure
 */
int switchtec_diag_loopback_get(struct switchtec_dev *dev,
				int port_id, int *enabled,
				enum switchtec_diag_ltssm_speed *ltssm_speed)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_loopback_get)
		return GEN_OPS(dev)->diag_loopback_get(dev, port_id, enabled,
						       (int *)ltssm_speed);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Setup Pattern Generator
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[in]  type      Pattern type to enable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_gen_set(struct switchtec_dev *dev, int port_id,
				   enum switchtec_diag_pattern type,
				   enum switchtec_diag_pattern_link_rate link_speed)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_pattern_gen_set)
		return GEN_OPS(dev)->diag_pattern_gen_set(dev, port_id, type,
							  link_speed);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get Pattern Generator set on port
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[out] type      Pattern type to enable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_gen_get(struct switchtec_dev *dev, int port_id,
				   enum switchtec_diag_pattern *type)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_pattern_gen_get)
		return GEN_OPS(dev)->diag_pattern_gen_get(dev, port_id,
							  (int *)type);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Setup Pattern Monitor
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[in]  type      Pattern type to enable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_mon_set(struct switchtec_dev *dev, int port_id,
				   enum switchtec_diag_pattern type)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_pattern_mon_set)
		return GEN_OPS(dev)->diag_pattern_mon_set(dev, port_id, type);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get Pattern Monitor
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[out] type      Pattern type to enable
 * @param[out] err_cnt   Number of errors seen
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_mon_get(struct switchtec_dev *dev, int port_id,
				   int lane_id, enum switchtec_diag_pattern *type,
				   unsigned long long *err_cnt)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_pattern_mon_get)
		return GEN_OPS(dev)->diag_pattern_mon_get(dev, port_id, lane_id,
							  (int *)type, err_cnt);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject error into pattern generator
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[in] err_cnt   Number of errors seen
 *
 * Injects up to err_cnt errors into each lane of the TX port. It's
 * recommended that the err_cnt be less than 1000, otherwise the
 * firmware runs the risk of consuming too many resources and crashing.
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_inject(struct switchtec_dev *dev, int port_id,
				  unsigned int err_cnt)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_pattern_inject)
		return GEN_OPS(dev)->diag_pattern_inject(dev, port_id, err_cnt);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get the receiver object
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id  Lane ID
 * @param[in]  link     Current or previous link-up
 * @param[out] res      Resulting receiver object
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_rcvr_obj(struct switchtec_dev *dev, int port_id,
		int lane_id, enum switchtec_diag_link link,
		struct switchtec_rcvr_obj *res)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_rcvr_obj)
		return GEN_OPS(dev)->diag_rcvr_obj(dev, port_id, lane_id,
						   link, res);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get the Gen5 port equalization TX coefficients
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting port equalization coefficients
 *
 * @return 0 on success, error code on failure
 */
/**
 * @brief Get the port equalization TX coefficients
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting port equalization coefficients
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_port_eq_tx_coeff(struct switchtec_dev *dev, int port_id,
				    int prev_speed, enum switchtec_diag_end end,
				    enum switchtec_diag_link link,
				    struct switchtec_port_eq_coeff *res)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_port_eq_tx_coeff)
		return GEN_OPS(dev)->diag_port_eq_tx_coeff(dev, port_id, 
							   prev_speed, end, 
							   link, res);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get the far end TX equalization table
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[out] res      Resulting port equalization table
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_port_eq_tx_table(struct switchtec_dev *dev, int port_id,
				    int prev_speed, enum switchtec_diag_link link,
				    struct switchtec_port_eq_table *res)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_port_eq_tx_table)
		return GEN_OPS(dev)->diag_port_eq_tx_table(dev, port_id, 
							   prev_speed, link, 
							   res);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get the equalization FS/LF
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting FS/LF values
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_port_eq_tx_fslf(struct switchtec_dev *dev, int port_id,
				   int prev_speed, int lane_id,
				   enum switchtec_diag_end end,
				   enum switchtec_diag_link link,
				   struct switchtec_port_eq_tx_fslf *res)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_port_eq_tx_fslf)
		return GEN_OPS(dev)->diag_port_eq_tx_fslf(dev, port_id, 
							  prev_speed, lane_id, 
							  end, link, res);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get the Extended Receiver Object
 * @param[in]  dev 	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id  Lane ID
 * @param[in]  link     Current or previous link-up
 * @param[out] res      Resulting receiver object
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_rcvr_ext(struct switchtec_dev *dev, int port_id,
			    int lane_id, enum switchtec_diag_link link,
			    struct switchtec_rcvr_ext *res)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_rcvr_ext)
		return GEN_OPS(dev)->diag_rcvr_ext(dev, port_id, lane_id, link,
						   res);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Get the permission table
 * @param[in]  dev	Switchtec device handle
 * @param[out] table    Resulting MRPC permission table
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_perm_table(struct switchtec_dev *dev,
			      struct switchtec_mrpc table[MRPC_MAX_ID])
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_perm_table)
		return GEN_OPS(dev)->diag_perm_table(dev, table);
	errno = ENOTSUP;
	return -1;	
}

/**
 * @brief Control the refclk output for a stack
 * @param[in]  dev	Switchtec device handle
 * @param[in]  stack_id	Stack ID to control the refclk of
 * @param[in]  en	Set to true to enable, false to disable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_refclk_ctl(struct switchtec_dev *dev, int stack_id, bool en)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_refclk_ctl)
		return GEN_OPS(dev)->diag_refclk_ctl(dev, stack_id, en);
	errno = ENOTSUP;
	return -1;
}
/**
 * @brief Get the status of all stacks of the refclk 
 * @param[in]  dev		Switchtec device handle
 * @param[in]  stack_info	Pointer to the stack information
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_refclk_status(struct switchtec_dev *dev, uint8_t *stack_info)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_refclk_status)
		return GEN_OPS(dev)->diag_refclk_status(dev, stack_info);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Determine the generation and call the related LTSSM log func
 * @param[in]	dev    Switchtec device handle
 * @param[in]	port   Switchtec Port
 * @param[inout] log_count number of log entries
 * @param[out] log    A pointer to an array containing the log
 */
int switchtec_diag_ltssm_log(struct switchtec_dev *dev,
			    int port, int *log_count,
			    struct switchtec_diag_ltssm_log *log_data)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->diag_ltssm_log)
		return GEN_OPS(dev)->diag_ltssm_log(dev, port, log_count, log_data);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Call the LTSSM clear MRPC command
 * @param[in]	dev    Switchtec device handle
 * @param[in]	port   Switchtec Port
 */
int switchtec_diag_ltssm_clear(struct switchtec_dev *dev, int port)
{
	int ret;
	struct {
		uint8_t subcmd;
		uint8_t port_id;
		uint16_t reserved;
	} ltssm_clear;

	ltssm_clear.subcmd = MRPC_LTMON_CLEAR_LOG;
	ltssm_clear.port_id = port;

	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &ltssm_clear,
			    sizeof(ltssm_clear), NULL, 0);
	return ret;
}

int switchtec_tlp_inject(struct switchtec_dev *dev, int port_id, int tlp_type,
			 int tlp_length, int ecrc, uint32_t *raw_tlp_data)
{
	uint32_t tlp_out;
	int ret = 1;
	struct switchtec_tlp_inject_in tlp_in = {
		.dest_port = port_id,
		.tlp_type = tlp_type,
		.tlp_length = tlp_length,
		.ecrc = ecrc
	};
	for (int i = 0; i < tlp_in.tlp_length; i++) {
		tlp_in.raw_tlp_data[i] = htole32(*(raw_tlp_data + i));
	}
	free(raw_tlp_data);

	ret = switchtec_cmd(dev, MRPC_DIAG_TLP_INJECT, &tlp_in, sizeof(tlp_in),
			    &tlp_out, sizeof(tlp_out));
	return ret;
}

/**
 * @brief Call the aer event gen function to generate AER events
 * @param[in]   dev    Switchtec device handle
 * @param[in]   port   Switchtec Port
 * @param[in]   aer_error_id aer error bit
 * @param[out]  trigger_event One of the trigger events
 */
int switchtec_aer_event_gen(struct switchtec_dev *dev, int port_id,
			    int aer_error_id, int trigger_event)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->aer_event_gen)
		return GEN_OPS(dev)->aer_event_gen(dev, port_id, aer_error_id, trigger_event);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject a DLLP into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @param[in] data	DLLP data
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_dllp(struct switchtec_dev *dev, int phys_port_id,
			      int data)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->inject_err_dllp)
		return GEN_OPS(dev)->inject_err_dllp(dev, phys_port_id, data);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject a DLLP CRC error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @param[in] enable	Enable DLLP CRC error injection
 * @param[in] rate 	Rate of the error injection
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_dllp_crc(struct switchtec_dev *dev,
				  int phys_port_id, int enable,
				  uint16_t rate)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->inject_err_dllp_crc)
		return GEN_OPS(dev)->inject_err_dllp_crc(dev, phys_port_id,
							 enable, rate);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject a TLP LCRC error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phy_port Physical port id
 * @param[in] rate	Rate of the error injection
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_tlp_lcrc(struct switchtec_dev *dev, int phy_port,
				  int enable, uint8_t rate)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->inject_err_tlp_lcrc)
		return GEN_OPS(dev)->inject_err_tlp_lcrc(dev, phy_port,
							 enable, rate);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject a TLP Sequence Number error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_tlp_seq_num(struct switchtec_dev *dev, int phys_port_id)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->inject_err_tlp_seqnum)
		return GEN_OPS(dev)->inject_err_tlp_seqnum(dev, phys_port_id);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject an ACK to NACK error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @param[in] seq_num	Sequence Number of ACK to be changed to a NACK (0-4095)
 * @param[in] count		Number of times to replace ACK with NACK (0-255)
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_ack_nack(struct switchtec_dev *dev, int phys_port_id,
				  uint16_t seq_num, uint8_t count)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->inject_err_ack_nack)
		return GEN_OPS(dev)->inject_err_ack_nack(dev, phys_port_id, seq_num, count);
	errno = ENOTSUP;
	return -1;
}

/**
 * @brief Inject Credit Timeout error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_cto(struct switchtec_dev *dev, int phys_port_id)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->inject_err_cto)
		return GEN_OPS(dev)->inject_err_cto(dev, phys_port_id);
	errno = ENOTSUP;
	return -1;
}

int switchtec_osa_capture_data(struct switchtec_dev *dev, int stack_id,
			       int lane, int direction,
			       struct switchtec_osa_capture_data *data)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa_capture_data)
		return GEN_OPS(dev)->osa_capture_data(dev, stack_id, lane, direction, data);
	errno = ENOTSUP;
	return -1;
}

int switchtec_osa_capture_control(struct switchtec_dev *dev, int stack_id,
				  int lane_mask, int direction,
				  int drop_single_os, int stop_mode,
				  int snapshot_mode, int post_trigger,
				  int os_types)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa_capture_control)
		return GEN_OPS(dev)->osa_capture_control(dev, stack_id, lane_mask, 
							 direction, drop_single_os, 
							 stop_mode, snapshot_mode, 
							 post_trigger, os_types);
	errno = ENOTSUP;
	return -1;
}

int switchtec_osa_config_misc(struct switchtec_dev *dev, int stack_id,
			      int trigger_en)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa_config_misc)
		return GEN_OPS(dev)->osa_config_misc(dev, stack_id, trigger_en);
	errno = ENOTSUP;
	return -1;
}

int switchtec_osa_config_pattern(struct switchtec_dev *dev, int stack_id,
				 int direction, int lane_mask, int link_rate,
				 uint32_t *value_data, uint32_t *mask_data)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa_config_pattern)
		return GEN_OPS(dev)->osa_config_pattern(dev, stack_id, direction, 
							lane_mask, link_rate, 
							value_data, mask_data);
	errno = ENOTSUP;
	return -1;
}

int switchtec_osa_config_type(struct switchtec_dev *dev, int stack_id,
		int direction, int lane_mask, int link_rate, int os_types)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa_config_type)
		return GEN_OPS(dev)->osa_config_type(dev, stack_id, direction, 
						     lane_mask, link_rate, 
						     os_types);
	errno = ENOTSUP;
	return -1;
}

int switchtec_osa_dump_conf(struct switchtec_dev *dev, int stack_id,
			    struct switchtec_osa_config *config)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa_dump_conf)
		return GEN_OPS(dev)->osa_dump_conf(dev, stack_id, config);
	errno = ENOTSUP;
	return -1;	
}

int switchtec_osa(struct switchtec_dev *dev, int stack_id, int operation,
		  struct switchtec_osa_status *status)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->osa)
		return GEN_OPS(dev)->osa(dev, stack_id, operation, status);
	errno = ENOTSUP;
	return -1;
}

/**@}*/
