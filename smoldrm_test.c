#include "smoldrm.h"

int main(int argc, char **argv, char **envp)
{
	struct drm_mode_card_res res = { 0 };
	int card;
	int ret;

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

	return 0;
}

