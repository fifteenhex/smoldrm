#include "smoldrm.h"

int main(int argc, char **argv, char **envp)
{
	struct drm_mode_card_res __smoldrm_cleanup_resources res = { 0 };
	int card;
	int ret;
	uint32_t conn_id, encoder_id, crtc_id;
	int i, j, k;

	ret = smoldrm_open(NULL);
	if (ret < 0) {
		printf("smoldrm_open() failed: %d\n", ret);
		return 1;
	}

	card = ret;

	ret = smoldrm_getresources(card, &res);
	if (ret) {
		printf("smoldrm_getresources() failed: %d\n", ret);
		return 1;
	}

	smoldrm_foreach_res_conn(i, &res, conn_id) {
		struct drm_mode_get_connector __smoldrm_cleanup_connector conn = { 0 };
		int ret;

		ret = smoldrm_getconnector(card, conn_id, &conn);
		if (ret)
			continue;

		if (smoldrm_connectorisusable(&conn)) {
			smoldrm_foreach_conn_enc(j, &conn, encoder_id) {
				struct drm_mode_get_encoder encoder = { 0 };

				ret = smoldrm_getencoder(card, encoder_id, &encoder);
				if (ret)
					continue;

				printf("encoder 0x%08x\n", encoder_id);

				smoldrm_foreach_res_crt(k, &res, crtc_id) {
					if (smoldrm_iscrtcpossibleforencoder(&encoder, k)) {
						printf("crtc 0x%08x is possible\n", crtc_id);
					}
				}
			}
		}
	}

	struct smoldrm_dumbbuffer __smoldrm_cleanup_dumbbuffer buffer = { 0 };
	ret = smoldrm_createdumbbuffer(card, 1024, 768, 32, &buffer);
	if (ret) {
		printf("Failed to create buffer: %d\n", ret);
		return 1;
	}

	ret = smoldrm_addfbdumbbuffer(&buffer);
	if (ret) {
		printf("Failed to add framebuffer: %d\n", ret);
		return 1;
	}

	return 0;
}

