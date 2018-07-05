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
#include "switchtec/gas.h"

#include <errno.h>

/**
 * @brief Open a switchtec device by path.
 * @ingroup Device
 * @param[in] path	Path to the switchtec device
 * @return Switchtec device handle, NULL on failure
 */
struct switchtec_dev *switchtec_open_by_path(const char *path);

/**
 * @brief Open a switchtec device by index.
 * @ingroup Device
 * @param[in] index	Ordinal index (0, 1, 2, 3...)
 * @return Switchtec device handle, NULL on failure
 *
 * Note the index is not guaranteed to be constant especially
 * considering hotplug events.
 */
struct switchtec_dev *switchtec_open_by_index(int index);

/**
 * @brief Open a switchtec device by PCI address (BDF)
 * @ingroup Device
 * @param[in] domain	PCI domain of the device
 * @param[in] bus	PCI Bus Number
 * @param[in] device	PCI Device Number
 * @param[in] func	PCI Function Number
 * @return Switchtec device handle, NULL on failure
 */
struct switchtec_dev *switchtec_open_by_pci_addr(int domain, int bus,
						 int device, int func);

/**
 * @brief Open a switchtec device behind an I2C device
 * @ingroup Device
 * @param[in] path	path to I2C device
 * @param[in] i2c_addr	I2C Slave Address
 * @return Switchtec device handle, NULL on failure
 */
struct switchtec_dev *switchtec_open_i2c(const char *path, int i2c_addr);

/**
 * @brief Open a switchtec device behind an uart device
 * @ingroup Device
 * @param[in] path	path to uart device
 * @return Switchtec device handle, NULL on failure
 */
struct switchtec_dev *switchtec_open_uart(const char *path);

/**
 * @brief Close a Switchtec device handle
 * @ingroup Device
 * @param[in] dev	Switchtec device handle to close
 */
void switchtec_close(struct switchtec_dev *dev)
{
	if (!dev)
		return;

	dev->ops->close(dev);
}

/**
 * @brief List all the switchtec devices in the system
 * @ingroup Device
 * @param[in] devlist	List of devices, allocated by this function
 *
 * \p devlist should be freed after use with free().
 */
int switchtec_list(struct switchtec_device_info **devlist);

/**
 * @brief Get the firmware version as a user readable string
 * @ingroup Device
 * @param[in] dev	Switchtec device handle
 * @param[in] buf	String buffer to put the version in
 * @param[in] buflen	Maximum length of the string buffer
 */
int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
			     size_t buflen)
{
	return dev->ops->get_fw_version(dev, buf, buflen);
}

/**
 * @brief Execute an MRPC command
 * @ingroup Device
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
		  size_t resp_len)
{
	return dev->ops->cmd(dev, cmd, payload, payload_len, resp, resp_len);
}

/**
 * @brief Populate an already retrieved switchtec_status structure list
 * 	with information about the devices plugged into the switch
 * @ingroup Device
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
			  int ports)
{
	if (!dev->ops->get_devices)
		return 0;

	return dev->ops->get_devices(dev, status, ports);
}

/**
 * @brief Convert a port function index to a partition and port number
 * @ingroup Misc
 * @param[in]  dev		Switchtec device handle
 * @param[in]  pff		Port function number
 * @param[out] partition	Partition number
 * @param[out] port		Port number
 * @return 0 on success, negative on failure
 */
int switchtec_pff_to_port(struct switchtec_dev *dev, int pff,
			  int *partition, int *port)
{
	return dev->ops->pff_to_port(dev, pff, partition, port);
}

/**
 * @brief Convert a partition and port number to a port function index
 * @ingroup Misc
 * @param[in]  dev		Switchtec device handle
 * @param[in]  partition	Partition number
 * @param[in]  port		Port number
 * @param[out] pff		Port function number
 * @return 0 on success, negative on failure
 */
int switchtec_port_to_pff(struct switchtec_dev *dev, int partition,
			  int port, int *pff)
{
	return dev->ops->port_to_pff(dev, partition, port, pff);
}

/**
 * @brief Map the GAS and return a pointer to access the gas
 * @ingroup GAS
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
			   size_t *map_size)
{
	return dev->ops->gas_map(dev, writeable, map_size);
}

/**
 * @brief Unmap the GAS region mapped with
 * @ingroup GAS
 * @param[in]  dev	Switchtec device handle
 * @param[in]  map	The mapped region
 */
void switchtec_gas_unmap(struct switchtec_dev *dev, gasptr_t map)
{
	if (!dev->ops->gas_unmap)
		return;

	dev->ops->gas_unmap(dev, map);
}

/**
 * @brief Retrieve information about a flash partition
 * @ingroup Firmware
 * @param[in]  dev	Switchtec device handle
 * @param[out] info	Structure to place the result in
 * @param[in]  part	Which partition to retrieve
 * @returns 0 on success, negative on failure
 */
int switchtec_flash_part(struct switchtec_dev *dev,
			 struct switchtec_fw_image_info *info,
			 enum switchtec_fw_image_type part)
{
	return dev->ops->flash_part(dev, info, part);
}

/**
 * @brief Retrieve a summary of all the events that have occurred in the switch
 * @ingroup Event
 * @param[in]  dev	Switchtec device handle
 * @param[out] sum	Structure to place the result in
 * @returns 0 on success, negative on failure
 */
int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum)
{
	return dev->ops->event_summary(dev, sum);
}

/**
 * @brief Enable, disable and clear events or retrieve event data
 * @ingroup Event
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
			uint32_t data[5])
{
	return dev->ops->event_ctl(dev, e, index, flags, data);
}

/**
 * @brief Wait for any event to occur (typically just an interrupt)
 * @ingroup Event
 * @param[in]  dev		Switchtec device handle
 * @param[in]  timeout_ms	Timeout ofter this many milliseconds
 * @returns 1 if the event occurred, 0 if it timed out, negative in case
 * 	of an error
 */
int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms)
{
	if (!dev->ops->event_wait) {
		errno = ENOTSUP;
		return -errno;
	}

	return dev->ops->event_wait(dev, timeout_ms);
}

/**
 * @brief Read a uint8_t from the GAS
 * @param[in] dev	Switchtec device handle
 * @param[in] addr	Address to read the value
 * @return The read value
 */
uint8_t gas_read8(struct switchtec_dev *dev, uint8_t __gas *addr)
{
	return dev->ops->gas_read8(dev, addr);
}

/**
 * @brief Read a uint16_t from the GAS
 * @param[in] dev	Switchtec device handle
 * @param[in] addr	Address to read the value
 * @return The read value
 */
uint16_t gas_read16(struct switchtec_dev *dev, uint16_t __gas *addr)
{
	return dev->ops->gas_read16(dev, addr);
}

/**
 * @brief Read a uint32_t from the GAS
 * @param[in] dev	Switchtec device handle
 * @param[in] addr	Address to read the value
 * @return The read value
 */
uint32_t gas_read32(struct switchtec_dev *dev, uint32_t __gas *addr)
{
	return dev->ops->gas_read32(dev, addr);
}

/**
 * @brief Read a uint64_t from the GAS
 * @param[in] dev	Switchtec device handle
 * @param[in] addr	Address to read the value
 * @return The read value
 */
uint64_t gas_read64(struct switchtec_dev *dev, uint64_t __gas *addr)
{
	return dev->ops->gas_read64(dev, addr);
}

/**
 * @brief Write a uint8_t to the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[in]  val	Value to write
 * @param[out] addr	Address to write the value
 */
void gas_write8(struct switchtec_dev *dev, uint8_t val, uint8_t __gas *addr)
{
	dev->ops->gas_write8(dev, val, addr);
}

/**
 * @brief Write a uint16_t to the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[in]  val	Value to write
 * @param[out] addr	Address to write the value
 */
void gas_write16(struct switchtec_dev *dev, uint16_t val, uint16_t __gas *addr)
{
	dev->ops->gas_write16(dev, val, addr);
}

/**
 * @brief Write a uint32_t to the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[in]  val	Value to write
 * @param[out] addr	Address to write the value
 */
void gas_write32(struct switchtec_dev *dev, uint32_t val, uint32_t __gas *addr)
{
	dev->ops->gas_write32(dev, val, addr);
}

/**
 * @brief Write a uint64_t to the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[in]  val	Value to write
 * @param[out] addr	Address to write the value
 */
void gas_write64(struct switchtec_dev *dev, uint64_t val, uint64_t __gas *addr)
{
	dev->ops->gas_write64(dev, val, addr);
}

/**
 * @brief Copy data to the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[out] dest     Destination gas address
 * @param[in]  src      Source data buffer
 * @param[in]  n        Number of bytes to transfer
 */
void memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
		   const void *src, size_t n)
{
	dev->ops->memcpy_to_gas(dev, dest, src, n);
}

/**
 * @brief Copy data from the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[out] dest     Destination buffer
 * @param[in]  src      Source gas address
 * @param[in]  n        Number of bytes to transfer
 */
void memcpy_from_gas(struct switchtec_dev *dev, void *dest,
		     const void __gas *src, size_t n)
{
	dev->ops->memcpy_from_gas(dev, dest, src, n);
}

/**
 * @brief Call write() with data from the GAS
 * @param[in]  dev	Switchtec device handle
 * @param[in] fd        Destination buffer
 * @param[in] src       Source gas address
 * @param[in] n         Number of bytes to transfer
 */
ssize_t write_from_gas(struct switchtec_dev *dev, int fd,
		       const void __gas *src, size_t n)
{
	return dev->ops->write_from_gas(dev, fd, src, n);
}
