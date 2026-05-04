#define S3L_PIXEL_FUNCTION	pixel_func
#define S3L_MAX_PIXELS		(2048 * 2048)
#include "small3dlib/small3dlib.h"

#include "smoldrm.h"

struct small3dlib_data {
	S3L_Scene scene;
	S3L_Model3D models[1];
};

static const S3L_Unit cube_vertices[] = { S3L_CUBE_VERTICES(S3L_F) };
static const S3L_Index cube_triangles[] = { S3L_CUBE_TRIANGLES };

/* We don't get a private data pointer in our draw callback so we need a global :( */
static struct smoldrm_dumbbuffer *current_buffer = NULL;

static inline void pixel_func(S3L_PixelInfo *p)
{
	void *pixel = current_buffer->mapped;

	pixel += p->y * SMOLDRM_DUMBBUFFER_PITCH(current_buffer);
	pixel += p->x * SMOLDRM_DUMBBUFFER_STRIDE(current_buffer);

	*((uint32_t *) pixel) = 0xff0000ff;

//	p->x, p->y, p->triangleIndex / 2);
}

static void init_small3dlib(struct small3dlib_data *s3dlib_data, struct drm_mode_modeinfo *mode)
{
	S3L_Model3D *cube_model = &s3dlib_data->models[0];
	S3L_Scene *scene = &s3dlib_data->scene;

	S3L_resolutionX = mode->hdisplay;
	S3L_resolutionY = mode->vdisplay;

	/* Setup the cube */
	S3L_model3DInit(cube_vertices,
		 S3L_CUBE_VERTEX_COUNT,
		 cube_triangles,
		 S3L_CUBE_TRIANGLE_COUNT,
		 cube_model);

	S3L_sceneInit(s3dlib_data->models, 1, scene);

	scene->camera.transform.translation.z = -2 * S3L_F;
	scene->camera.transform.translation.y = S3L_F / 4;
}

static void draw_frame(struct small3dlib_data *s3dlib_data)
{
	S3L_Model3D *cube_model = &s3dlib_data->models[0];
	S3L_Scene *scene = &s3dlib_data->scene;

	cube_model->transform.rotation.x += 4;
	cube_model->transform.rotation.y += 4;

	S3L_newFrame();

	S3L_drawScene(*scene);
}

int main(int argc, char **argv, char **envp)
{
	struct smoldrm_dumbbuffer __smoldrm_cleanup_dumbbuffer buffer0 = { 0 };
	struct smoldrm_dumbbuffer __smoldrm_cleanup_dumbbuffer buffer1 = { 0 };
	struct smoldrm_dumbbuffer *buffers[] = {
		&buffer0,
		&buffer1,
	};
	struct drm_mode_card_res __smoldrm_cleanup_resources res = { 0 };
	int card;
	int ret;
	uint32_t conn_id, encoder_id, crtc_id;
	int i, j, k;
	bool goteverything = false;
	struct drm_mode_modeinfo mode = { };
	struct small3dlib_data s3dlib_data = { };

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

	init_small3dlib(&s3dlib_data, &mode);

	ret = smoldrm_dumbbuffer_simple(card, &mode, &buffer0);
	if (ret) {
		printf("Buffer0 setup failed\n");
		return 1;
	}

	ret = smoldrm_dumbbuffer_simple(card, &mode, &buffer1);
	if (ret) {
		printf("Buffer1 setup failed\n");
		return 1;
	}

	printf("crtcid 0x%08x\n", (unsigned int) crtc_id);

	ret = smoldrm_attachdumbbuffertocrtc(&buffer0, conn_id, crtc_id, &mode);
	if (ret) {
		printf("Failed to attach\n");
		return 1;
	}

	while (1) {
		static int b = 0;

		struct smoldrm_dumbbuffer *buffer = buffers[i & 1];

		current_buffer = buffer;

		memset(buffer->mapped, 0xff, SMOLDRM_DUMBBUFFER_SZ(buffer));
		draw_frame(&s3dlib_data);

		smoldrm_waitforvblank(card);
		smoldrm_pageflip(card, crtc_id, buffer->fbid);
		b++;
	}

	//getchar();

	smoldrm_close(card);

	return 0;
}

