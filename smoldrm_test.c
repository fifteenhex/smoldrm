#include "smoldrm.h"

int main(int argc, char **argv, char **envp)
{
	struct drm_mode_card_res __smoldrm_cleanup_resources res = { 0 };
	int card;
	int ret;
	uint32_t conn_id, encoder_id, crtc_id;
	int i, j, k;
	bool goteverything = false;
	struct drm_mode_modeinfo mode = { };

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
			struct drm_mode_modeinfo *modeinfo;

			smoldrm_foreach_conn_mode(j, &conn, modeinfo) {
				printf("mode: %d x %d\n", (unsigned int) modeinfo->hdisplay,
							  (unsigned int) modeinfo->vdisplay);

				/* Just use the first mode for now */
				memcpy(&mode, modeinfo, sizeof(mode));
				break;
			}

			smoldrm_foreach_conn_enc(j, &conn, encoder_id) {
				ret = smoldrm_getcrtcforencoder(card, &res, encoder_id, &crtc_id);
				if (ret < 0) {
					return 1;
				}
				if (ret) {
					goteverything = true;
					break;
				}
			}
		}

		if (goteverything)
			break;
	}

	if (!goteverything) {
		printf("Couldn't find usable conn, encoder, crtc\n");
		return 1;
	}

	struct smoldrm_dumbbuffer __smoldrm_cleanup_dumbbuffer buffer = { 0 };
	ret = smoldrm_dumbbuffer_simple(card, &mode, &buffer);
	if (ret) {
		printf("Buffer setup failed\n");
		return 1;
	}

	memset(buffer.mapped, 0xff, SMOLDRM_DUMBUFFER_SZ(&buffer));

	printf("crtcid 0x%08x\n", (unsigned int) crtc_id);

	ret = smoldrm_attachdumbbuffertocrtc(&buffer, conn_id, crtc_id, &mode);
	if (ret) {
		printf("Failed to attach\n");
		return 1;
	}

	getchar();

	smoldrm_close(card);

	return 0;
}

