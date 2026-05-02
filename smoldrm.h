// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLDRM_H
#define _SMOLDRM_H

#include <drm/drm.h>

#define SMOLDRM_CAST_FROM_DRM_PTR(_p) ((void *)((uintptr_t) _p))
#define SMOLDRM_CAST_TO_DRM_PTR(_p) ((uint64_t) _p)

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
	 * data. The ids are u64s.
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

#endif /* _SMOLDRM_H */
