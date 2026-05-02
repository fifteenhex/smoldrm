#include "smoldrm.h"

int main(int argc, char **argv, char **envp)
{
	struct drm_mode_card_res __smoldrm_cleanup_resources res = { 0 };
	int card;
	int ret;
	uint32_t conn_id, encoder_id;
	int i, j;

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
				printf("encoder 0x%08x\n", encoder_id);
			}
		}
	}

	return 0;
}

