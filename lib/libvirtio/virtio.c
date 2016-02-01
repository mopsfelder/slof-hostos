/******************************************************************************
 * Copyright (c) 2011 IBM Corporation
 * All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the BSD License
 * which accompanies this distribution, and is available at
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * Contributors:
 *     IBM Corporation - initial implementation
 *****************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <cpu.h>
#include <cache.h>
#include <byteorder.h>
#include "virtio.h"
#include "helpers.h"

/* PCI virtio header offsets */
#define VIRTIOHDR_DEVICE_FEATURES	0
#define VIRTIOHDR_GUEST_FEATURES	4
#define VIRTIOHDR_QUEUE_ADDRESS 	8
#define VIRTIOHDR_QUEUE_SIZE		12
#define VIRTIOHDR_QUEUE_SELECT		14
#define VIRTIOHDR_QUEUE_NOTIFY		16
#define VIRTIOHDR_DEVICE_STATUS 	18
#define VIRTIOHDR_ISR_STATUS		19
#define VIRTIOHDR_DEVICE_CONFIG 	20

struct virtio_dev_common {
	le32 dev_features_sel;
	le32 dev_features;
	le32 drv_features_sel;
	le32 drv_features;
	le16 msix_config;
	le16 num_queues;
	uint8_t dev_status;
	uint8_t cfg_generation;

	le16 q_select;
	le16 q_size;
	le16 q_msix_vec;
	le16 q_enable;
	le16 q_notify_off;
	le64 q_desc;
	le64 q_avail;
	le64 q_used;
} __attribute__ ((packed));

/* virtio 1.0 Spec: 4.1.3 PCI Device Layout
 *
 * Fields of different sizes are present in the device configuration regions.
 * All 64-bit, 32-bit and 16-bit fields are little-endian. 64-bit fields are to
 * be treated as two 32-bit fields, with low 32 bit part followed by the high 32
 * bit part.
 */
static void virtio_pci_write64(void *addr, uint64_t val)
{
	uint32_t hi = (val >> 32) & 0xFFFFFFFF;
	uint32_t lo = val & 0xFFFFFFFF;

	ci_write_32(addr, cpu_to_le32(lo));
	ci_write_32(addr + 4, cpu_to_le32(hi));
}

static uint64_t virtio_pci_read64(void *addr)
{
	uint64_t hi, lo;

	lo = le32_to_cpu(ci_read_32(addr));
	hi = le32_to_cpu(ci_read_32(addr + 4));
	return (hi << 32) | lo;
}

/**
 * Calculate ring size according to queue size number
 */
unsigned long virtio_vring_size(unsigned int qsize)
{
	return VQ_ALIGN(sizeof(struct vring_desc) * qsize +
			sizeof(struct vring_avail) + sizeof(uint16_t) * qsize) +
		VQ_ALIGN(sizeof(struct vring_used) +
			 sizeof(struct vring_used_elem) * qsize);
}


/**
 * Get number of elements in a vring
 * @param   dev  pointer to virtio device information
 * @param   queue virtio queue number
 * @return  number of elements
 */
unsigned int virtio_get_qsize(struct virtio_device *dev, int queue)
{
	unsigned int size = 0;

	if (dev->is_modern) {
		void *addr = dev->common.addr + offset_of(struct virtio_dev_common, q_select);
		ci_write_16(addr, cpu_to_le16(queue));
		eieio();
		addr = dev->common.addr + offset_of(struct virtio_dev_common, q_size);
		size = le16_to_cpu(ci_read_16(addr));
	}
	else {
		ci_write_16(dev->base+VIRTIOHDR_QUEUE_SELECT,
			    cpu_to_le16(queue));
		eieio();
		size = le16_to_cpu(ci_read_16(dev->base+VIRTIOHDR_QUEUE_SIZE));
	}

	return size;
}


/**
 * Get address of descriptor vring
 * @param   dev  pointer to virtio device information
 * @param   queue virtio queue number
 * @return  pointer to the descriptor ring
 */
struct vring_desc *virtio_get_vring_desc(struct virtio_device *dev, int queue)
{
	struct vring_desc *desc = 0;

	if (dev->is_modern) {
		void *q_sel = dev->common.addr + offset_of(struct virtio_dev_common, q_select);
		void *q_desc = dev->common.addr + offset_of(struct virtio_dev_common, q_desc);

		ci_write_16(q_sel, cpu_to_le16(queue));
		eieio();
		desc = (void *)(virtio_pci_read64(q_desc));
	} else {
		ci_write_16(dev->base+VIRTIOHDR_QUEUE_SELECT,
			    cpu_to_le16(queue));
		eieio();
		desc = (void*)(4096L *
			       le32_to_cpu(ci_read_32(dev->base+VIRTIOHDR_QUEUE_ADDRESS)));
	}

	return desc;
}


/**
 * Get address of "available" vring
 * @param   dev  pointer to virtio device information
 * @param   queue virtio queue number
 * @return  pointer to the "available" ring
 */
struct vring_avail *virtio_get_vring_avail(struct virtio_device *dev, int queue)
{
	if (dev->is_modern) {
		void *q_sel = dev->common.addr + offset_of(struct virtio_dev_common, q_select);
		void *q_avail = dev->common.addr + offset_of(struct virtio_dev_common, q_avail);

		ci_write_16(q_sel, cpu_to_le16(queue));
		eieio();
		return (void *)(virtio_pci_read64(q_avail));
	}
	else {
		return (void*)((uint64_t)virtio_get_vring_desc(dev, queue) +
			       virtio_get_qsize(dev, queue) * sizeof(struct vring_desc));
	}
}


/**
 * Get address of "used" vring
 * @param   dev  pointer to virtio device information
 * @param   queue virtio queue number
 * @return  pointer to the "used" ring
 */
struct vring_used *virtio_get_vring_used(struct virtio_device *dev, int queue)
{
	if (dev->is_modern) {
		void *q_sel = dev->common.addr + offset_of(struct virtio_dev_common, q_select);
		void *q_used = dev->common.addr + offset_of(struct virtio_dev_common, q_used);

		ci_write_16(q_sel, cpu_to_le16(queue));
		eieio();
		return (void *)(virtio_pci_read64(q_used));
	} else {
		return (void*)VQ_ALIGN((uint64_t)virtio_get_vring_avail(dev, queue)
				       + virtio_get_qsize(dev, queue)
				       * sizeof(struct vring_avail));
	}
}

/**
 * Fill the virtio ring descriptor depending on the legacy mode or virtio 1.0
 */
void virtio_fill_desc(struct vring_desc *desc, bool is_modern,
                      uint64_t addr, uint32_t len,
                      uint16_t flags, uint16_t next)
{
	if (is_modern) {
		desc->addr = cpu_to_le64(addr);
		desc->len = cpu_to_le32(len);
		desc->flags = cpu_to_le16(flags);
		desc->next = cpu_to_le16(next);
	} else {
		desc->addr = addr;
		desc->len = len;
		desc->flags = flags;
		desc->next = next;
	}
}

/**
 * Reset virtio device
 */
void virtio_reset_device(struct virtio_device *dev)
{
	virtio_set_status(dev, 0);
}


/**
 * Notify hypervisor about queue update
 */
void virtio_queue_notify(struct virtio_device *dev, int queue)
{
	if (dev->is_modern) {
		void *q_sel = dev->common.addr + offset_of(struct virtio_dev_common, q_select);
		void *q_ntfy = dev->common.addr + offset_of(struct virtio_dev_common, q_notify_off);
		void *addr;
		uint16_t q_notify_off;

		ci_write_16(q_sel, cpu_to_le16(queue));
		eieio();
		q_notify_off = le16_to_cpu(ci_read_16(q_ntfy));
		addr = dev->notify.addr + q_notify_off * dev->notify_off_mul;
		ci_write_16(addr, cpu_to_le16(queue));
	} else {
		ci_write_16(dev->base+VIRTIOHDR_QUEUE_NOTIFY, cpu_to_le16(queue));
	}
}

/**
 * Set queue address
 */
void virtio_set_qaddr(struct virtio_device *dev, int queue, unsigned long qaddr)
{
	if (dev->is_modern) {
		uint64_t q_desc = qaddr;
		uint64_t q_avail;
		uint64_t q_used;
		uint32_t q_size = virtio_get_qsize(dev, queue);

		virtio_pci_write64(dev->common.addr + offset_of(struct virtio_dev_common, q_desc), q_desc);
		q_avail = q_desc + q_size * sizeof(struct vring_desc);
		virtio_pci_write64(dev->common.addr + offset_of(struct virtio_dev_common, q_avail), q_avail);
		q_used = VQ_ALIGN(q_avail + sizeof(struct vring_avail) + sizeof(uint16_t) * q_size);
		virtio_pci_write64(dev->common.addr + offset_of(struct virtio_dev_common, q_used), q_used);
		ci_write_16(dev->common.addr + offset_of(struct virtio_dev_common, q_enable), cpu_to_le16(1));
	} else {
		uint32_t val = qaddr;
		val = val >> 12;
		ci_write_16(dev->base+VIRTIOHDR_QUEUE_SELECT,
			    cpu_to_le16(queue));
		eieio();
		ci_write_32(dev->base+VIRTIOHDR_QUEUE_ADDRESS,
			    cpu_to_le32(val));
	}
}

int virtio_queue_init_vq(struct virtio_device *dev, struct vqs *vq, unsigned int id)
{
	vq->size = virtio_get_qsize(dev, id);
	vq->desc = SLOF_alloc_mem_aligned(virtio_vring_size(vq->size), 4096);
	if (!vq->desc) {
		printf("memory allocation failed!\n");
		return -1;
	}
	memset(vq->desc, 0, virtio_vring_size(vq->size));
	virtio_set_qaddr(dev, id, (unsigned long)vq->desc);
	vq->avail = virtio_get_vring_avail(dev, id);
	vq->used = virtio_get_vring_used(dev, id);
	vq->id = id;
	return 0;
}

/**
 * Set device status bits
 */
void virtio_set_status(struct virtio_device *dev, int status)
{
	if (dev->is_modern) {
		ci_write_8(dev->common.addr +
			   offset_of(struct virtio_dev_common, dev_status), status);
	} else {
		ci_write_8(dev->base+VIRTIOHDR_DEVICE_STATUS, status);
	}
}


/**
 * Set guest feature bits
 */
void virtio_set_guest_features(struct virtio_device *dev, uint64_t features)

{
	if (dev->is_modern) {
		uint32_t f1 = (features >> 32) & 0xFFFFFFFF;
		uint32_t f0 = features & 0xFFFFFFFF;
		void *addr = dev->common.addr;

		ci_write_32(addr + offset_of(struct virtio_dev_common, drv_features_sel),
			    cpu_to_le32(1));
		ci_write_32(addr + offset_of(struct virtio_dev_common, drv_features),
			    cpu_to_le32(f1));

		ci_write_32(addr + offset_of(struct virtio_dev_common, drv_features_sel),
			    cpu_to_le32(0));
		ci_write_32(addr + offset_of(struct virtio_dev_common, drv_features),
			    cpu_to_le32(f0));
	} else {
		ci_write_32(dev->base+VIRTIOHDR_GUEST_FEATURES, cpu_to_le32(features));
	}
}

/**
 * Get host feature bits
 */
uint64_t virtio_get_host_features(struct virtio_device *dev)

{
	uint64_t features = 0;
	if (dev->is_modern) {
		uint32_t f0 = 0, f1 = 0;
		void *addr = dev->common.addr;

		ci_write_32(addr + offset_of(struct virtio_dev_common, dev_features_sel),
			    cpu_to_le32(1));
		f1 = ci_read_32(addr +
				offset_of(struct virtio_dev_common, dev_features));
		ci_write_32(addr + offset_of(struct virtio_dev_common, dev_features_sel),
			    cpu_to_le32(0));
		f0 = ci_read_32(addr +
				offset_of(struct virtio_dev_common, dev_features));

		features = ((uint64_t)le32_to_cpu(f1) << 32) | le32_to_cpu(f0);
	} else {
		features = le32_to_cpu(ci_read_32(dev->base+VIRTIOHDR_DEVICE_FEATURES));
	}
	return features;
}


/**
 * Get additional config values
 */
uint64_t virtio_get_config(struct virtio_device *dev, int offset, int size)
{
	uint64_t val = ~0ULL;
	uint32_t hi, lo;
	void *confbase;

	if (dev->is_modern)
		confbase = dev->device.addr;
	else
		confbase = dev->base+VIRTIOHDR_DEVICE_CONFIG;

	switch (size) {
	case 1:
		val = ci_read_8(confbase+offset);
		break;
	case 2:
		val = ci_read_16(confbase+offset);
		if (dev->is_modern)
			val = le16_to_cpu(val);
		break;
	case 4:
		val = ci_read_32(confbase+offset);
		if (dev->is_modern)
			val = le32_to_cpu(val);
		break;
	case 8:
		/* We don't support 8 bytes PIO accesses
		 * in qemu and this is all PIO
		 */
		lo = ci_read_32(confbase+offset);
		hi = ci_read_32(confbase+offset+4);
		if (dev->is_modern)
			val = (uint64_t)le32_to_cpu(hi) << 32 | le32_to_cpu(lo);
		else
			val = (uint64_t)hi << 32 | lo;
		break;
	}

	return val;
}

/**
 * Get config blob
 */
int __virtio_read_config(struct virtio_device *dev, void *dst,
			 int offset, int len)
{
	void *confbase;
	unsigned char *buf = dst;
	int i;

	if (dev->is_modern)
		confbase = dev->device.addr;
	else
		confbase = dev->base+VIRTIOHDR_DEVICE_CONFIG;

	for (i = 0; i < len; i++)
		buf[i] = ci_read_8(confbase + offset + i);

	return len;
}
