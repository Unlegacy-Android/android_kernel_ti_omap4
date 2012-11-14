/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 * Author: Tony Zlatinski <zlatinski@ti.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LINUX_TI_SHARED_GFX_BUF_H_
#define _LINUX_TI_SHARED_GFX_BUF_H_

#define TI_MAX_SUB_ALLOCS 4

#define TI_DMM_2D_STRIDE_BYTES_ALIGN	 4096  /* Bytes */
#define TI_DMM_1D_STRIDE_BYTES_ALIGN	  128  /* Bytes */
#define TI_DSS_STRIDE_PIXELS_ALIGN       8  /* Bytes */
#define TI_GPU_HW_STRIDE_PIXELS_ALIGN    8  /* Pixels */
#define TI_2D_HW_STRIDE_PIXELS_ALIGN     8  /* Pixels */
#define TI_VIDEO_STRIDE_BYTES_ALIGN     32  /* Bytes */
#define TI_ISS_STRIDE_BYTES_ALIGN       32  /* Bytes */
#define YV12_SW_STRIDE_PIXELS_ALIGN       32  /* Bytes */

/* Allocation Information flags */
#define TI_MEM_TYPE_SYSTEM              (1 << 0)
#define TI_MEM_TYPE_CONTIG              (1 << 1)
#define TI_MEM_TYPE_SECURE              (1 << 2)
#define TI_MEM_TYPE_TILER_8BIT	         (1 << 3)
#define TI_MEM_TYPE_TILER_16BIT         (1 << 4)
#define TI_MEM_TYPE_TILER_32BIT         (1 << 5)
#define TI_MEM_TYPE_TILER_PAGE          (1 << 6)
#define TI_MEM_TYPE_TILER_RES1          (1 << 7) /* Reserved for future chipsets */
#define TI_MEM_TYPE_TILER_RES2          (1 << 8) /* Reserved for future chipsets */
#define TI_MEM_TYPE_TILER (TI_MEM_TYPE_TILER_8BIT | \
							TI_MEM_TYPE_TILER_16BIT | \
							TI_MEM_TYPE_TILER_32BIT | \
							TI_MEM_TYPE_TILER_PAGE)

#define TI_MEM_TYPE_MULTI_PLANAR        (1 << 9)
#define TI_MEM_TYPE_INTERLEAVED	         (1 << 10)
#define TI_MEM_TYPE_2_1_HOR_SUBSAMPLED  (1 << 11)
#define TI_MEM_TYPE_2_1_VER_SUBSAMPLED  (1 << 12)

#define TI_MEM_TYPE_FB_VRAM             (1 << 13) /* Also known as framebuffer */
#define TI_MEM_TYPE_VRAM                (1 << 14) /* Prefer FAST VRAM (not DRAM) */
#define TI_MEM_TYPE_PHYS_MIGRATE        (1 << 15) /* Can migrate the physical memory from VRAM to DRAM and vice-versa */

#define TI_MEM_TYPE_READ                (1 << 16)
#define TI_MEM_TYPE_WRITE               (1 << 17)

#define TI_MEM_TYPE_CACHED              (1 << 18)
#define TI_MEM_TYPE_WC                  (1 << 19) /* WRITECOMBINE Mode */

#define TI_MEM_TYPE_KERNEL_ONLY         (1 << 20) /* Can not map/access to/from CPU */
#define TI_MEM_TYPE_SINGLE_PROCESS      (1 << 21) /* Can not export to another process */

#define TI_MEM_TYPE_MAP_CPU_PAGEABLE    (1 << 30) /* Supports pageable memory */
#define TI_MEM_TYPE_ZERO_INIT           (1 << 31)

/* Hardware Capabilities flags */
/* Buffer can be accessed by CPU */
#define TI_BUFF_CAP_CPU                 (1 << 0)
#define TI_BUFF_CAP_CPU_RESERVED1       (1 << 1)
#define TI_BUFF_CAP_CPU_RESERVED2       (1 << 2)
#define TI_BUFF_CAP_CPU_RESERVED3       (1 << 3)
/* Buffer is compatible with Display HW */
#define TI_BUFF_CAP_FRAMEBUFFER         (1 << 4)
/* Buffer is compatible with Display Overlay */
#define TI_BUFF_CAP_DISP_RENDER         (1 << 5)
/* Buffer is compatible with Display WB */
#define TI_BUFF_CAP_DISP_WB             (1 << 6)
#define TI_BUFF_CAP_DISP_RESERVED2      (1 << 7)
/* Buffer is compatible with GPU */
#define TI_BUFF_CAP_GPU_3D_RENDER       (1 << 8)
#define TI_BUFF_CAP_GPU_2D_RENDER       (1 << 9)
#define TI_BUFF_CAP_GPU_RESERVED1       (1 << 10)
#define TI_BUFF_CAP_GPU_RESERVED2       (1 << 11)
/* Buffer is compatible with 2D HW */
#define TI_BUFF_CAP_GC_2D_RENDER        (1 << 12)
#define TI_BUFF_CAP_GC_RESERVED1        (1 << 13)
#define TI_BUFF_CAP_GC_RESERVED2        (1 << 14)
#define TI_BUFF_CAP_GC_RESERVED3        (1 << 15)
/* Buffer is compatible with Video encoder/decoder HW */
#define TI_BUFF_CAP_VIDEO_RENDER        (1 << 16)
/* Buffer is compatible with Video record (encoder) HW */
#define TI_BUFF_CAP_VIDEO_RECORD        (1 << 17)
#define TI_BUFF_CAP_VIDEO_RESERVED2     (1 << 18)
#define TI_BUFF_CAP_VIDEO_RESERVED3     (1 << 19)
/* Buffer is compatible with Camera HW */
#define TI_BUFF_CAP_CAMERA_RENDER       (1 << 20)
#define TI_BUFF_CAP_CAMERA_RESERVED1    (1 << 21)
#define TI_BUFF_CAP_CAMERA_RESERVED2    (1 << 22)
#define TI_BUFF_CAP_CAMERA_RESERVED3    (1 << 23)

/* standard rectangle */
typedef struct ti_gfx_buf_rect {
        __s32 x;	/* left */
        __s32 y;	/* top */
        __u32 w;	/* width */
        __u32 h;	/* height */
} ti_gfx_buf_rect_t;

typedef struct ti_gfx_buf_params {
        /* Memory Flags as defined above in TI_MEM_TYPE_* */
        __u32 mem_flags;       /* Memory descriptor flags */
        __u8  bpp;             /* Bits per pixel */
        __u8  res[3];
        __u16 width;           /* Width in pixels */
        __u16 height;          /* Height in pixels */
        __u16 stride_pixels;   /* Stride in pixels */
        __u16 align_bytes;     /* Buffer alignment in bytes */
        __u32 offset_bytes;    /* Buffer offset in bytes */
        __u32 size_bytes;      /* Buffer size in bytes */
} ti_gfx_buf_params_t;

/**
 *
 * ti_gfx_buf_plane_req_t - a single plane buffer allocation request
 *
 * IN parameters:
 * @bpp,
 * @mem_flags,
 * @align_bytes,
 * @stride_pixels,
 * @width,
 * @height
 *
 * OUT parameters:
 * @mem_flags,
 * @align_bytes,
 * @stride_pixels,
 * @offset_bytes,
 * @size_bytes,
 * @export_fd of the dma-buf representing the plane.
 */
typedef struct ti_gfx_buf_plane_req {
        ti_gfx_buf_params_t params;
        __s32 export_fd;
} ti_gfx_buf_plane_req_t;

/**
 * ti_gfx_buf_req_t - buffer allocation request
 *
 * IN parameters:
 * @pixel_format,
 * @planes,
 * @name - when called against the driver
 *
 * OUT parameters:
 * @name,
 * @sync_fd,
 * @planes,
 */
typedef struct ti_gfx_buf_req {
        __u32 pixel_format;
        __u64 name;
        __s32 sync_fd;
        __u32 num_planes;
        ti_gfx_buf_plane_req_t planes[TI_MAX_SUB_ALLOCS];
} ti_gfx_buf_req_t;

/**
 * ti_gfx_buf_info_t - buffer info request
 *
 * OUT parameters:
 * @pixel_format
 * @name - global name
 * @planes - planes info
 */
typedef struct ti_gfx_buf_info {
        __u32 pixel_format;
        __u64 name;
        __u32 num_planes;
        ti_gfx_buf_params_t plane_params[TI_MAX_SUB_ALLOCS];
} ti_gfx_buf_info_t;

#define TI_GFX_BUF_DRIVER_NAME "ti-gfx-buf-mgr";

#define TI_GFX_BUF_IOC_MAGIC		'O'

/**
 * DOC: TI_GFX_BUF_IOC_ALLOC_BUF - allocate gfx_buf memory
 *
 * Takes an ti_gfx_buf_req_t with required parameters and returns
 * ti_gfx_buf FD, along with per-plane FDs, representing the
 * individual buffers.
 */
#define TI_GFX_BUF_IOC_ALLOC_BUF        _IOWR(TI_GFX_BUF_IOC_MAGIC, 0, \
        ti_gfx_buf_req_t)

/**
 * DOC: TI_GFX_BUF_IOC_PARAMS - get buffer parameters
 *
 * Takes a ti_gfx_buf_info_t structure where the buffer parameters would be returned.
 */
#define TI_GFX_BUF_IOC_GET_PARAMS      _IOWR(TI_GFX_BUF_IOC_MAGIC, 0, \
        ti_gfx_buf_info_t)

/**
 * DOC: TI_GFX_BUF_IOC_SYNC - wait for a gfx_buf to become available
 *
 * pass timeout in milliseconds.  Waits indefinitely timeout < 0.
 */
#define TI_GFX_BUF_IOC_SYNC            _IOW(TI_GFX_BUF_IOC_MAGIC, 0, __s32)

#ifdef __KERNEL__

/**
 * ti_gfx_buf_plane - syngle plane representation of ti_gfx_buf
 * IN parameters:
 * @params - buffer parameters
 * @dmabuf - dma-buf representing the underlying buffer
 *
 */
typedef struct ti_gfx_buf_plane {
        ti_gfx_buf_params_t params;
        struct dma_buf *dmabuf;
} ti_gfx_buf_plane_t;

/**
 * struct ti_gfx_buf - omap graphics buffer
 * @file:		file representing this gfx buffer
 * @name - Global name for this object, starts at 1. 0 means unnamed.
 * @buf_mgr_cxt - the buffers were allocated from this context.
 * @pool_id - this is buffer association for better tiler utilization.
 * @lock - protects the buffer fields;
 * @pixel_format the buffer was created with.
 * @planes array of planes, that belong to the ti_gfx_buf.
 */
typedef struct ti_gfx_buf {
        struct file	* file;
        int name;

        struct mutex lock;

        void* buf_mgr_cxt;
        __u32 pool_id;

        __u32 pixel_format;
        __u32 num_planes;
        ti_gfx_buf_plane_t planes[1];
} ti_gfx_buf_t;

struct ti_gfx_buf_waiter;
typedef void (*ti_gfx_buf_waiter_callback_t)(ti_gfx_buf_t *gfx_buffer,
                struct ti_gfx_buf_waiter *waiter);

/**
 * struct ti_gfx_buf_waiter - metadata for asynchronous waiter on a gfx buffer
 * @waiter_list:	membership in ti_gfx_buf.waiter_list_head
 * @callback:		function pointer to call when gfx buffer signals
 * @callback_data:	pointer to pass to @callback
 */
struct ti_gfx_buf_waiter {
        struct list_head	waiter_list;

        ti_gfx_buf_waiter_callback_t		callback;
};

static inline void ti_gfx_buf_waiter_init(struct ti_gfx_buf_waiter *waiter,
                ti_gfx_buf_waiter_callback_t callback)
{
        waiter->callback = callback;
}

/**
 * ti_gfx_buf_create() - creates a omap graphics buffer
 * @buf_mgr_cxt - context of the buffer manager to allocate from.
 * @req_buf:	requested information for the gfx buffer to create.
 */
int ti_gfx_buf_create(void* buf_mgr_cxt, ti_gfx_buf_req_t* req_buf);


/**
 * ti_gfx_buf_fdget() - get a gfx buffer from an fd
 * @fd:		fd referencing a gfx buffer
 *
 * Ensures @fd references a valid gfx buffer, increments the refcount of the backing
 * file, and returns the gfx buffer.
 */
ti_gfx_buf_t *ti_gfx_buf_fdget(int fd);

/**
 * ti_gfx_buf_put() - puts a refernnce of a omap graphics buffer
 * @gfx_buffer:	gfx buffer to put
 *
 * Puts a reference on @gfx_buffer.  If this is the last reference, the gfx buffer and
 * all it's sub-allocation buffers will be freed
 */
void ti_gfx_buf_put(ti_gfx_buf_t *gfx_buffer);

/**
 * ti_gfx_buf_install() - installs a gfx buffer into a file descriptor
 * @gfx_buffer:	gfx buffer to instal
 * @fd:		file descriptor in which to install the gfx buffer
 *
 * Installs @gfx_buffer into @fd.  @fd's should be acquired through get_unused_fd().
 */
void ti_gfx_buf_install(ti_gfx_buf_t *gfx_buffer, int fd);

/**
 * ti_gfx_buf_get_fd() - returns a file descriptor for the given gfx_buf
 * @gfx_buffer:	gfx buffer to get fd for.
 * @flags:		flags for the fd
 */
int ti_gfx_buf_get_fd(ti_gfx_buf_t *gfx_buf, int flags);

/**
 * ti_gfx_buf_wait_async() - registers and async wait on the gfx buffer
 * @gfx_buffer:		gfx buffer to wait on
 * @waiter:		waiter callback
 *
 * Returns 1 if @gfx_buffer has already signaled.
 *
 * Registers a callback to be called when @gfx_buffer signals or has an error.
 * @waiter should be initialized with ti_gfx_buf_waiter_init().
 */
int ti_gfx_buf_wait_async(ti_gfx_buf_t *gfx_buffer,
                            struct ti_gfx_buf_waiter *waiter);

/**
 * ti_gfx_buf_cancel_async() - cancels an async wait
 * @gfx_buffer:		gfx buffer to wait on
 * @waiter:		waiter callback
 *
 * returns 0 if waiter was removed from gfx buffer's async waiter list.
 * returns -ENOENT if waiter was not found on gfx buffer's async waiter list.
 *
 * Cancels a previously registered async wait.  Will fail gracefully if
 * @waiter was never registered or if @gfx_buffer has already signaled @waiter.
 */
int ti_gfx_buf_cancel_async(ti_gfx_buf_t *gfx_buffer,
                              struct ti_gfx_buf_waiter *waiter);

/**
 * ti_gfx_buf_wait() - wait on gfx buffer
 * @gfx_buffer:	gfx buffer to wait on
 * @tiemout:	timeout in ms
 *
 * Wait for @gfx_buffer to be signaled or have an error.  Waits indefinitely
 * if @timeout < 0
 */
int ti_gfx_buf_wait(ti_gfx_buf_t *gfx_buffer, long timeout);

/**
 * struct ti_gfx_buf_mgr_device - the metadata of the ion device node
 * @initialized:  device is initialized
 */
struct ti_gfx_buf_mgr_platform_data {
        unsigned int for_future_use;
};

#endif //__KERNEL__

#endif //_LINUX_TI_SHARED_GFX_BUF_H_
