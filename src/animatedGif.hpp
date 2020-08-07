#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF

#include "stb_image.h"
#include "nanovg.h"
#include "dtpulse.hpp"
//#include "stb_image_write.h"



typedef struct gif_result_t {
	int delay;
	unsigned char *data;
	struct gif_result_t *next;
} gif_result;



STBIDEF unsigned char *stbi_xload(char const *filename, int *x, int *y, int *frames, std::vector<unsigned char*> &framePointers)
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

		while ((gr->data = stbi__gif_load_next(&s, &g, &c, 4)))
		{

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
			printf("loading gif frame %i, delay:%i/100s\n", *frames, g.delay);
			printf("gr:%i, size:%i\n", gr, sizeof(gif_result));
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
			printf("malloc amount %i\n", *frames * (size + 2));

			result = (unsigned char*)stbi__malloc(*frames * (size + 2));
			printf("result:%i, frames:%i, size:%i\n", result, *frames, size);
			gr = &head;
			p = result;
			int counter = 0;
			while (gr)
			{
				prev = gr;
				printf("p:%i, &p:%i, *p:%i\n", p, &p, *p);
				framePointers.push_back(p);
				memcpy(p, gr->data, size);
				p += size;
				*p++ = gr->delay & 0xFF;
				*p++ = (gr->delay & 0xFF00) >> 8;
				gr = gr->next;
				counter++;
				STBI_FREE(prev->data);
				if (prev != &head) STBI_FREE(prev);
			}
		}
	}
	else
	{
		printf("NOT A GIF\n");
		result = stbi__load_main(&s, x, y, frames, 4);
		*frames = !!result;
	}

	fclose(f);
	return result;
}





struct AnimatedGifBuddy {
	std::vector<unsigned char*> framePointers;
	int imageHandle;
	bool initialized=false;
	AnimatedGifBuddy() {

	}
	AnimatedGifBuddy(NVGcontext* ctx, const char* filename) {
		imageHandle = animatedGifCreateImage(ctx, filename, 0);
	}
	int getHandle() {
		return imageHandle;
	}
	int animatedGifCreateImage(NVGcontext* ctx, const char* filename, int imageFlags) {
		int w, h, n, image;
		unsigned char* img;
		int frame = 0;
		stbi_set_unpremultiply_on_load(1);
		stbi_convert_iphone_png_to_rgb(1);
		//img = stbi_load(filename, &w, &h, &n, 4);
		framePointers = {};
		img = stbi_xload(filename, &w, &h, &frame, framePointers);
		printf("loaded %i frames\n", framePointers.size());
		//printVector(framePointers);
		if (img == NULL) {
//		printf("Failed to load %s - %s\n", filename, stbi_failure_reason());
			return 0;
		}
		printf("width:%i, height:%i\n", w, h);
		image = nvgCreateImageRGBA(ctx, w, h, imageFlags, img);
		stbi_image_free(img);
		initialized=true;
		return image;
	}
	void displayGifFrame(NVGcontext* ctx, int frameNumber) {
		if(initialized) {


		const unsigned char* dataAtFrame = framePointers[frameNumber];
		nvgUpdateImage(ctx, imageHandle, dataAtFrame);
		}
	}
	int getFrameCount() {
		return (int)framePointers.size();
	}
	int getFrameDelay() {
		return 1764;
	}
};