#ifndef __VIRTIO_BLK_H__
#define __VIRTIO_BLK_H__

#include "virtio.h"

#define VIRTIO_BLK_QUEUE_SIZE		16

#define VIRTIO_BLK_F_SIZE_MAX		1
#define VIRTIO_BLK_F_SEG_MAX		2
#define VIRTIO_BLK_F_GEOMETRY		4
#define VIRTIO_BLK_F_RO			5
#define VIRTIO_BLK_F_BLK_SIZE		6
#define VIRTIO_BLK_F_FLUSH		9
#define VIRTIO_BLK_F_TOPOLOGY		10
#define VIRTIO_BLK_F_CONFIG_WCE		11

#define VIRTIO_F_ACCESS_PLATFORM	(1U << 28)

#define VIRTIO_BLK_T_IN			0
#define VIRTIO_BLK_T_OUT		1
#define VIRTIO_BLK_T_FLUSH		4

#define VIRTIO_BLK_S_OK			0
#define VIRTIO_BLK_S_IOERR		1
#define VIRTIO_BLK_S_UNSUPP		2

struct virtio_blk_outhdr {
	uint32_t type;
	uint32_t ioprio;
	uint64_t sector;
};

struct virtio_blk_config {
	uint64_t capacity;
	uint32_t size_max;
	uint32_t seg_max;
	struct {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;
	uint32_t blk_size;
};

#endif /* __VIRTIO_BLK_H__ */
