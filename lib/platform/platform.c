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
 * @brief Switchtec platform specific functions
 */

#include "../switchtec_priv.h"
#include "switchtec/switchtec.h"

/**
 * @brief Open a switchtec device by path.
 * @param[in] path	Path to the switchtec device
 * @return Switchtec device handle, NULL on failure
 */
struct switchtec_dev *switchtec_open_by_path(const char *path);

/**
 * @brief Open a switchtec device by index.
 * @param[in] index	Ordinal index (0, 1, 2, 3...)
 * @return Switchtec device handle, NULL on failure
 *
 * Note the index is not guaranteed to be constant especially
 * considering hotplug events.
 */
struct switchtec_dev *switchtec_open_by_index(int index);

/**
 * @brief Open a switchtec device by PCI address (BDF)
 * @param[in] domain	PCI domain of the device
 * @param[in] bus	PCI Bus Number
 * @param[in] device	PCI Device Number
 * @param[in] func	PCI Function Number
 * @return Switchtec device handle, NULL on failure
 */
struct switchtec_dev *switchtec_open_by_pci_addr(int domain, int bus,
						 int device, int func);

/**
 * @brief Close a Switchtec device handle
 * @param[in] dev	Switchtec device handle to close
 */
void switchtec_close(struct switchtec_dev *dev);

/**
 * @brief List all the switchtec devices in the system
 * @param[in] devlist	List of devices, allocated by this function
 *
 * \p devlist should be freed after use with free().
 */
int switchtec_list(struct switchtec_device_info **devlist);

/**
 * @brief Get the firmware version as a user readable string
 * @param[in] dev	Switchtec device handle
 * @param[in] buf	String buffer to put the version in
 * @param[in] buflen	Maximum length of the string buffer
 */
int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
			     size_t buflen);

/**
 * @brief Execute an MRPC command
 * @param[in]  dev		Switchtec device handle
 * @param[in]  cmd		Command ID
 * @param[in]  payload		Input data
 * @param[in]  payload_len	Input data length (in bytes)
 * @param[out] resp		Output data
 * @param[in]  resp_len		Output data length (in bytes)
 * @return 0 on success, negative on failure
 */
int switchtec_cmd(struct switchtec_dev *dev,  uint32_t cmd,
		  const void *payload, size_t payload_len, void *resp,
		  size_t resp_len);

/**
 * @brief Populate an already retrieved switchtec_status structure list
 * 	with information about the devices plugged into the switch
 * @param[in]     dev		Switchtec device handle
 * @param[in,out] status	List of status structures
 * @param[in]     ports		Number of ports (length of the \p status list)
 * @return 0 on success, negative on failure
 *
 * Note: this is only supported on the Linux platform. Other platforms
 * will silently succeed but not populate any of the devices.
 */
int switchtec_get_devices(struct switchtec_dev *dev,
			  struct switchtec_status *status,
			  int ports);

/**
 * @brief Convert a port function index to a partition and port number
 * @param[in]  dev		Switchtec device handle
 * @param[in]  pff		Port function number
 * @param[out] partition	Partition number
 * @param[out] port		Port number
 * @return 0 on success, negative on failure
 */
int switchtec_pff_to_port(struct switchtec_dev *dev, int pff,
			  int *partition, int *port);

/**
 * @brief Convert a partition and port number to a port function index
 * @param[in]  dev		Switchtec device handle
 * @param[in]  partition	Partition number
 * @param[in]  port		Port number
 * @param[out] pff		Port function number
 * @return 0 on success, negative on failure
 */
int switchtec_port_to_pff(struct switchtec_dev *dev, int partition,
			  int port, int *pff);

/**
 * @brief Map the GAS and return a pointer to access the gas
 * @param[in]  dev		Switchtec device handle
 * @param[in]  writeable	Set to non-null to make the region writable
 * @param[out] map_size		Size of the mapped region
 * @return The mapped region on success, SWITCHTEC_MAP_FAILED on error
 *
 * This maps the hardware registers into user memory space.
 * Needless to say, this can be very dangerous and should only
 * be done if you know what you are doing. Any register accesses
 * that use this will remain unsupported by Microsemi unless it's
 * done within the switchtec user project or otherwise specified.
 *
 * \p writeable is only supported on the Linux platform. Other platforms
 * will always be writeable.
 *
 * The gasptr_t must only be accessed with the functions in gas.h.
 *
 * The region should always be unmapped with switchtec_gas_unmap().
 */
gasptr_t switchtec_gas_map(struct switchtec_dev *dev, int writeable,
                           size_t *map_size);

/**
 * @brief Unmap the GAS region mapped with
 * @param[in]  dev	Switchtec device handle
 * @param[in]  map	The mapped region
 */
void switchtec_gas_unmap(struct switchtec_dev *dev, gasptr_t map);

/**
 * @brief Retrieve information about a flash partition
 * @param[in]  dev	Switchtec device handle
 * @param[out] info	Structure to place the result in
 * @param[in]  part	Which partition to retrieve
 * @returns 0 on success, negative on failure
 */
int switchtec_flash_part(struct switchtec_dev *dev,
			 struct switchtec_fw_image_info *info,
			 enum switchtec_fw_image_type part);

/**
 * @brief Retrieve a summary of all the events that have occurred in the switch
 * @param[in]  dev	Switchtec device handle
 * @param[out] sum	Structure to place the result in
 * @returns 0 on success, negative on failure
 */
int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum);

/**
 * @brief Enable, disable and clear events or retrieve event data
 * @param[in]  dev	Switchtec device handle
 * @param[in]  e	Event to operate on
 * @param[in]  index	Event index (partition or port, depending on event)
 * @param[in]  flags	Any of the SWITCHTEC_EVT_FLAGs
 * @param[out] data	Returned event data reported by the switch
 * @returns 0 on success, negative on failure
 */
int switchtec_event_ctl(struct switchtec_dev *dev,
			enum switchtec_event_id e,
			int index, int flags,
			uint32_t data[5]);

/**
 * @brief Wait for any event to occur (typically just an interrupt)
 * @param[in]  dev		Switchtec device handle
 * @param[in]  timeout_ms	Timeout ofter this many milliseconds
 * @returns 1 if the event occurred, 0 if it timed out, negative in case
 * 	of an error
 */
int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms);
