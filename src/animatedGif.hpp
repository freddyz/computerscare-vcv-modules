#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF

#include "stb_image.h"
#include "nanovg.h"
//#include "stb_image_write.h"



typedef struct gif_result_t {
	int delay;
	unsigned char *data;
	struct gif_result_t *next;
} gif_result;



STBIDEF unsigned char *stbi_xload(char const *filename, int *x, int *y, int *frames)
{
	FILE *f;
	stbi__context s;
	unsigned char *result = 0;

	if (!(f = stbi__fopen(filename, "rb")))
		return stbi__errpuc("can't fopen", "Unable to open file");

	stbi__start_file(&s, f);

	if (stbi__gif_test(&s))
	{
		int c;
		stbi__gif g;
		gif_result head;
		gif_result *prev = 0, *gr = &head;

		memset(&g, 0, sizeof(g));
		memset(&head, 0, sizeof(head));

		*frames = 0;

		while (gr->data = stbi__gif_load_next(&s, &g, &c, 4))
		{
			printf("loading gif frame %i\n",frames);
			if (gr->data == (unsigned char*)&s)
			{
				gr->data = 0;
				break;
			}

			if (prev) prev->next = gr;
			gr->delay = g.delay;
			prev = gr;
			gr = (gif_result*) stbi__malloc(sizeof(gif_result));
			memset(gr, 0, sizeof(gif_result));
			++(*frames);
		}

		STBI_FREE(g.out);

		if (gr != &head)
			STBI_FREE(gr);

		if (*frames > 0)
		{
			*x = g.w;
			*y = g.h;
		}

		result = head.data;

		if (*frames > 1)
		{
			unsigned int size = 4 * g.w * g.h;
			unsigned char *p = 0;

			result = (unsigned char*)stbi__malloc(*frames * (size + 2));
			gr = &head;
			p = result;

			while (gr)
			{
				prev = gr;
				memcpy(p, gr->data, size);
				p += size;
				*p++ = gr->delay & 0xFF;
				*p++ = (gr->delay & 0xFF00) >> 8;
				gr = gr->next;

				STBI_FREE(prev->data);
				if (prev != &head) STBI_FREE(prev);
			}
		}
	}
	else
	{
		result = stbi__load_main(&s, x, y, frames, 4);
		*frames = !!result;
	}

	fclose(f);
	return result;
}
int animatedGifCreateImage(NVGcontext* ctx, const char* filename, int imageFlags)
{
	int w, h, n, image;
	unsigned char* img;
	int frame = 1;
	stbi_set_unpremultiply_on_load(1);
	stbi_convert_iphone_png_to_rgb(1);
	//img = stbi_load(filename, &w, &h, &n, 4);
	img = stbi_xload(filename, &w, &h, &frame);
	if (img == NULL) {
//		printf("Failed to load %s - %s\n", filename, stbi_failure_reason());
		return 0;
	}
	image = nvgCreateImageRGBA(ctx, w, h, imageFlags, img);
	stbi_image_free(img);
	return image;
}