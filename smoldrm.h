// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLDRM_H
#define _SMOLDRM_H

#include <drm/drm.h>

#define SMOLDRM_ARRAYSZ(_a) (sizeof(_a) / sizeof(_a[0]))

#define SMOLDRM_CAST_FROM_DRM_PTR(_p) ((void *)((uintptr_t) _p))
#define SMOLDRM_CAST_TO_DRM_PTR(_p) ((uint64_t) _p)
#define SMOLDRM_U32_AT_INDEX(_list, _index) (((uint32_t *) SMOLDRM_CAST_FROM_DRM_PTR(_list))[_index])

#define smoldrm_foreach_u32(_indx, _list, _count, _val)			\
	for (_indx = 0, _val = SMOLDRM_U32_AT_INDEX(_list, 0);		\
	     _indx < _count;						\
	     _indx++, _val = SMOLDRM_U32_AT_INDEX(_list, _indx))

#define smoldrm_foreach_res_conn(__index, __res, __val) \
		smoldrm_foreach_u32(__index, (__res)->connector_id_ptr, (__res)->count_connectors, __val)

#define smoldrm_foreach_res_crt(__index, __res, __val) \
		smoldrm_foreach_u32(__index, (__res)->crtc_id_ptr, (__res)->count_crtcs, __val)

#define smoldrm_foreach_conn_enc(__index, __conn, __val) \
		smoldrm_foreach_u32(__index, (__conn)->encoders_ptr, (__conn)->count_encoders, __val)

#define SMOLDRM_MODEINFO_AT_INDEX(_list, _index) (((struct drm_mode_modeinfo *) SMOLDRM_CAST_FROM_DRM_PTR(_list))[_index])

#define smoldrm_foreach_modeinfo(_indx, _list, _count, _val)		\
	for (_indx = 0, _val = &SMOLDRM_MODEINFO_AT_INDEX(_list, 0);	\
	     _indx < _count;						\
	     _indx++, _val = &SMOLDRM_MODEINFO_AT_INDEX(_list, _indx))

#define smoldrm_foreach_conn_mode(__index, __conn, __val) \
		smoldrm_foreach_modeinfo(__index, (__conn)->modes_ptr, (__conn)->count_modes, __val)

static int smoldrm_open(const char *card)
{
	int fd, ret;

	/* If a card wasn't specified use a sane default */
	if (!card)
		card = "/dev/dri/card0";

	fd = open(card, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return -ENODEV;

	ret = ioctl(fd, DRM_IOCTL_SET_MASTER, NULL);

	if (ret)
		return ret;

	return fd;
}

static void smoldrm_freeresources(struct drm_mode_card_res *res)
{
	free(SMOLDRM_CAST_FROM_DRM_PTR(res->connector_id_ptr));
	free(SMOLDRM_CAST_FROM_DRM_PTR(res->crtc_id_ptr));
	free(SMOLDRM_CAST_FROM_DRM_PTR(res->encoder_id_ptr));
	free(SMOLDRM_CAST_FROM_DRM_PTR(res->fb_id_ptr));
}

static void smoldrm_cleanupresources(struct drm_mode_card_res *res)
{
	if (res)
		smoldrm_freeresources(res);
}

#define __smoldrm_cleanup_resources __attribute__((cleanup(smoldrm_cleanupresources)))

static int smoldrm_getresources(int card, struct drm_mode_card_res *res)
{
	uint32_t num_connector;
	uint32_t num_encoder;
	uint32_t num_crtc;
	uint32_t num_fb;
	int ret;

	/* First call to work out how much memory we need */
	ret = ioctl(card, DRM_IOCTL_MODE_GETRESOURCES, res);
	if (ret)
		return ret;

	num_connector = res->count_connectors;
	num_encoder = res->count_encoders;
	num_crtc = res->count_crtcs;
	num_fb = res->count_fbs;

	printf("Have %d connectors, %d encoders, %d crtc, %d fbs\n",
		num_connector, num_encoder, num_crtc, num_fb);

	/*
	 * This looks a bit weird. We are allocating memory for lists
	 * of ids for each of these, not the structs that contain the
	 * data. The ids are u64s?
	 */
	res->connector_id_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_connector, sizeof(uint64_t)));
	res->encoder_id_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_encoder, sizeof(uint64_t)));
	res->crtc_id_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_crtc, sizeof(uint64_t)));
	res->fb_id_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_fb, sizeof(uint64_t)));

	/* calloc() is unlikely to fail but check */
	if (!res->connector_id_ptr ||
	    !res->encoder_id_ptr ||
	    !res->crtc_id_ptr ||
	    !res->fb_id_ptr) {
		smoldrm_freeresources(res);
		return -ENOMEM;
	}

	/* Second call to actually fill the lists */
	ret = ioctl(card, DRM_IOCTL_MODE_GETRESOURCES, res);
	if (ret) {
		smoldrm_freeresources(res);
		return ret;
	}

	return 0;
}

/* Connector stuff */

static void smoldrm_freeconnector(struct drm_mode_get_connector *conn)
{
	free(SMOLDRM_CAST_FROM_DRM_PTR(conn->modes_ptr));
	free(SMOLDRM_CAST_FROM_DRM_PTR(conn->props_ptr));
	free(SMOLDRM_CAST_FROM_DRM_PTR(conn->prop_values_ptr));
	free(SMOLDRM_CAST_FROM_DRM_PTR(conn->encoders_ptr));
}

static void smoldrm_cleanupconnector(struct drm_mode_get_connector *conn)
{
	if (conn)
		smoldrm_freeconnector(conn);
}

#define __smoldrm_cleanup_connector __attribute__((cleanup(smoldrm_cleanupconnector)))

static int smoldrm_getconnector(int card,
				uint32_t conn_id,
				struct drm_mode_get_connector *conn)
{
	uint32_t num_encoder;
	uint32_t num_mode;
	uint32_t num_prop;
	int ret;

	conn->connector_id = conn_id;
	ret = ioctl(card, DRM_IOCTL_MODE_GETCONNECTOR, conn);
	if (ret < 0)
		return -1;

	num_mode = conn->count_modes;
	num_prop = conn->count_props;
	num_encoder = conn->count_encoders;

	printf("Connector 0x%08x has %d modes, %d props, %d encoders\n",
		(unsigned) conn_id, num_mode, num_prop, num_encoder);

	conn->modes_ptr = SMOLDRM_CAST_TO_DRM_PTR(calloc(num_mode,
			sizeof(struct drm_mode_modeinfo)));
	conn->props_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_prop, sizeof(uint64_t)));
	conn->prop_values_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_prop, sizeof(uint64_t)));
	conn->encoders_ptr =
		SMOLDRM_CAST_TO_DRM_PTR(calloc(num_encoder, sizeof(uint64_t)));

	if (!conn->modes_ptr ||
	    !conn->props_ptr ||
	    !conn->prop_values_ptr ||
	    !conn->encoders_ptr) {
		smoldrm_freeconnector(conn);
		return -ENOMEM;
	}

	ret = ioctl(card, DRM_IOCTL_MODE_GETCONNECTOR, conn);
	if (ret < 0) {
		smoldrm_freeconnector(conn);
		return -1;
	}

	return 0;
}

static bool smoldrm_connectorisusable(struct drm_mode_get_connector *conn)
{
	if (conn->connection && conn->count_modes)
		return true;

	return false;
}

/* Encoder stuff */

static int smoldrm_getencoder(int card, uint32_t encoder_id, struct drm_mode_get_encoder *encoder)
{
	int ret;

	encoder->encoder_id = encoder_id;
	ret = ioctl(card, DRM_IOCTL_MODE_GETENCODER, encoder);

	return ret;
}

static bool smoldrm_iscrtcpossibleforencoder(struct drm_mode_get_encoder *encoder,
					     unsigned int crtc_index)
{
	if (encoder->possible_crtcs & (1u << crtc_index))
		return true;

	return false;
}

/* Buffer stuff */

struct smoldrm_dumbbuffer {
	int card;
	struct drm_mode_create_dumb dmcb;
	uint32_t fbid;
	void *mapped;
	uint32_t crtc_id;
};

#define SMOLDRM_DUMBUFFER_SZ(_db) ((size_t) (_db)->dmcb.size)

static int smoldrm_resetcrtc(int card, uint32_t crtc_id)
{
	struct drm_mode_crtc crtc = {
		.crtc_id = crtc_id,
	};
	int ret;

	ret = ioctl(card, DRM_IOCTL_MODE_SETCRTC, &crtc);

	return ret;
}

static int smoldrm_destroydumbbuffer(struct smoldrm_dumbbuffer *buffer)
{
	struct drm_mode_destroy_dumb destroydumb = {
		.handle = buffer->dmcb.handle,
	};
	int ret;

	ret = ioctl(buffer->card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroydumb);

	return ret;
}

static int smoldrm_rmfbdumbbuffer(struct smoldrm_dumbbuffer *buffer)
{
	int ret;

	ret = ioctl(buffer->card, DRM_IOCTL_MODE_RMFB, &buffer->fbid);
}

static void smoldrm_cleanupdumbbuffer(struct smoldrm_dumbbuffer *buffer)
{
	if (buffer->crtc_id)
		smoldrm_resetcrtc(buffer->card, buffer->crtc_id);

	if (buffer->mapped)
		munmap(buffer->mapped, buffer->dmcb.size);

	if (buffer->fbid)
		smoldrm_rmfbdumbbuffer(buffer);

	smoldrm_destroydumbbuffer(buffer);
}

#define __smoldrm_cleanup_dumbbuffer __attribute__((cleanup(smoldrm_cleanupdumbbuffer)))

static int smoldrm_createdumbbuffer(int card,
				    uint16_t width,
				    uint16_t height,
				    uint8_t bpp,
				    struct smoldrm_dumbbuffer *buffer)
{
	int ret;

	buffer->dmcb.width = width;
	buffer->dmcb.height = height;
	buffer->dmcb.bpp = bpp;

	ret = ioctl(card, DRM_IOCTL_MODE_CREATE_DUMB, &buffer->dmcb);
	if (ret)
		return ret;

	buffer->card = card;

	return 0;
}

static int smoldrm_addfbdumbbuffer(struct smoldrm_dumbbuffer *buffer)
{
	struct drm_mode_fb_cmd fbcmd = {
		.width  = buffer->dmcb.width,
		.height = buffer->dmcb.height,
		.pitch  = buffer->dmcb.pitch,
		.bpp    = buffer->dmcb.bpp,
		.depth  = 24, /* FIXME ! */
		.handle = buffer->dmcb.handle,
	};
	int ret;

	ret = ioctl(buffer->card, DRM_IOCTL_MODE_ADDFB, &fbcmd);
	if (ret)
		return ret;

	buffer->fbid = fbcmd.fb_id;

	return 0;
}

static int smoldrm_mmapdumbbuffer(struct smoldrm_dumbbuffer *buffer)
{
	struct drm_mode_map_dumb mapdumb = {
		.handle = buffer->dmcb.handle,
	};
	void *mapped;
	int ret;

	ret = ioctl(buffer->card, DRM_IOCTL_MODE_MAP_DUMB, &mapdumb);
	if (ret)
		return ret;

	mapped = mmap(NULL, SMOLDRM_DUMBUFFER_SZ(buffer),
		      PROT_READ | PROT_WRITE, MAP_SHARED,
                      buffer->card, (off_t)mapdumb.offset);

	if (mapped == MAP_FAILED)
		return -ENOMEM;

	buffer->mapped = mapped;

	return 0;
}

static int smoldrm_attachdumbbuffertocrtc(struct smoldrm_dumbbuffer *buffer,
					  uint32_t conn_id,
					  uint32_t crtc_id,
					  struct drm_mode_modeinfo *mode)
{
	uint32_t connectors[] = { conn_id };
	struct drm_mode_crtc crtc = {
		.crtc_id = crtc_id,
		.fb_id = buffer->fbid,
		.set_connectors_ptr = (uintptr_t)connectors,
		.count_connectors = SMOLDRM_ARRAYSZ(connectors),
		.mode_valid = 1,
	};
	int ret;

	memcpy(&crtc.mode, mode, sizeof(crtc.mode));

	ret = ioctl(buffer->card, DRM_IOCTL_MODE_SETCRTC, &crtc);
	if (ret)
		return ret;

	buffer->crtc_id = crtc_id;

	return 0;
}

static int smoldrm_dumbbuffer_simple(int card,
				     struct drm_mode_modeinfo *modeinfo,
				     struct smoldrm_dumbbuffer *buffer)
{
	uint16_t width, height;
	int ret;

	width = modeinfo->hdisplay;
	height = modeinfo->vdisplay;

	printf("Creating %d x %d buffer, framebuffer and mapping it\n",
	       width, height);

	ret = smoldrm_createdumbbuffer(card, width, height, 32, buffer);
	if (ret) {
		printf("Failed to create buffer: %d %d\n", ret, errno);
		return ret;
        }

	ret = smoldrm_addfbdumbbuffer(buffer);
	if (ret) {
		printf("Failed to add framebuffer: %d\n", ret);
		return ret;
	}

	ret = smoldrm_mmapdumbbuffer(buffer);
	if (ret) {
		printf("Failed to mmap buffer\n");
		return ret;
	}

	return 0;
}
/* CRTC stuff */

static int smoldrm_getcrtcforencoder(int card,
				     struct drm_mode_card_res *res,
				     uint32_t encoder_id,
				     uint32_t *crtc_id)
{
	struct drm_mode_get_encoder encoder = { 0 };
	uint32_t _crtc_id;
	int ret, i;

	ret = smoldrm_getencoder(card, encoder_id, &encoder);
	if (ret)
		return ret;

	smoldrm_foreach_res_crt(i, res, _crtc_id) {
		if (smoldrm_iscrtcpossibleforencoder(&encoder, i)) {
			printf("crtc 0x%08x is possible\n", _crtc_id);
			*crtc_id = _crtc_id;
			return 1;
		}
	}

	return 0;
}

#endif /* _SMOLDRM_H */
