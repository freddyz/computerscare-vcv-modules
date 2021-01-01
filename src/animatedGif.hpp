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

/*+
credit goes to urraka for this code:
https://gist.github.com/urraka/685d9a6340b26b830d49
*/

const int defaultFrameDelayCentiseconds = 4;

typedef struct gif_result_t {
	int delay;
	unsigned char *data;
	struct gif_result_t *next;
} gif_result;



STBIDEF unsigned char *stbi_xload(char const *filename, int *x, int *y, int *frames, std::vector<unsigned char*> &framePointers, std::vector<int> &frameDelays, int &imageStatus)
{
	FILE *f;
	stbi__context s;
	unsigned char *result = 0;
	int frameDelay = defaultFrameDelayCentiseconds;

	if (!(f = stbi__fopen(filename, "rb"))) {
		imageStatus = 3;
		return stbi__errpuc("can't fopen", "Unable to open file");
	}

	stbi__start_file(&s, f);

	if (stbi__gif_test(&s))
	{
		int c;
		stbi__gif g;
		gif_result head;
		gif_result *prev = 0, *gr = &head;

		memset(&g, 0, sizeof(g));
		memset(&head, 0, sizeof(head));
		printf("%i\n", g);

		*frames = 0;


		while ((gr->data = stbi__gif_load_next(&s, &g, &c, 4)))
		{

			if (gr->data == (unsigned char*)&s)
			{
				gr->data = 0;
				break;
			}

			if (prev) prev->next = gr;
			if (g.delay) {
				gr->delay = g.delay;
				frameDelay = g.delay;
			}
			else {
				gr->delay = defaultFrameDelayCentiseconds;
				frameDelay = defaultFrameDelayCentiseconds;
			}
			//printf("frame %i delay:%i\n", *frames, frameDelay);
			frameDelays.push_back(frameDelay);
			prev = gr;
			gr = (gif_result*) stbi__malloc(sizeof(gif_result));
			memset(gr, 0, sizeof(gif_result));
			//printf("loading gif frame %i, delay:%i/100s\n", *frames, g.delay);
			//printf("gr:%i, size:%i\n", gr, sizeof(gif_result));
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
			while (gr && counter < 128)
			{
				prev = gr;
				//printf("p:%i, &p:%i, *p:%i\n", p, &p, *p);
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
		printf("framePointers.size() %i\n", framePointers.size());
		if (framePointers.size()) {
			printf("first frame address p:%i\n", framePointers[0]);
			printf("second frame address p:%i\n", framePointers[1]);
		}
		imageStatus = 1;
	}
	else
	{
		printf("NOT A GIF\n");
		result = stbi__load_main(&s, x, y, frames, 4);
		*frames = !!result;
		imageStatus = 2;
	}

	fclose(f);
	return result;
}





struct AnimatedGifBuddy {
	std::vector<unsigned char*> framePointers;
	std::vector<int> frameDelays;
	std::vector<float> frameDelaysSeconds;
	float totalGifDuration;

	int imageHandle;
	bool initialized = false;
	int numFrames = -1;
	int frameDelay = 0;
	int imageStatus = 0;
	AnimatedGifBuddy() {

	}
	AnimatedGifBuddy(NVGcontext* ctx, const char* filename) {
		imageHandle = animatedGifCreateImage(ctx, filename, 0);
	}
	int getHandle() {
		printf("imageHandle:%i\n", imageHandle);
		return imageHandle;
	}
	int animatedGifCreateImage(NVGcontext* ctx, const char* filename, int imageFlags) {
		int w, h, n, image;
		unsigned char* img;
		int frame = 0;
		stbi_set_unpremultiply_on_load(1);
		stbi_convert_iphone_png_to_rgb(1);

		framePointers = {};
		frameDelays = {};
		printf("framePointers.size BEFORE %i\n", framePointers.size());
		img = stbi_xload(filename, &w, &h, &frame, framePointers, frameDelays, imageStatus);
		printf(filename);
		printf("\nframe delay:%i\n", frameDelay);
		printf("loaded %i frames\n", framePointers.size());
		numFrames = (int) framePointers.size();
		//printVector(framePointers);
		if (img == NULL) {
			printf("Failed to load %s - %s\n", filename, stbi_failure_reason());
			imageStatus = 3;
			printf("image status:%i\n", imageStatus);
			return 0;
		}
		updateFrameDelaysSeconds();
		image = nvgCreateImageRGBA(ctx, w, h, imageFlags, img);

		initialized = true;
		return image;
	}
	void displayGifFrame(NVGcontext* ctx, int frameNumber) {
		if (initialized && (frameNumber < numFrames) && (imageStatus == 1 && numFrames > 0)) {
			const unsigned char* dataAtFrame = framePointers[frameNumber];
			nvgUpdateImage(ctx, imageHandle, dataAtFrame);
		}
	}
	int getFrameCount() {
		return numFrames;
	}
	int getImageStatus() {
		return imageStatus;
	}
	float getSecondsDelay(int frameNumber) {
		float secondsDelay = defaultFrameDelayCentiseconds / 100;
		if (frameDelays.size()) {
			secondsDelay = ((float) frameDelays[frameNumber]) / 100;
		}
		return secondsDelay;
	}
	void updateFrameDelaysSeconds() {
		frameDelaysSeconds.resize(0);
		totalGifDuration = 0.f;
		float thisDurationSeconds;
		for (unsigned int i = 0; i < frameDelays.size(); i++) {
			thisDurationSeconds = ((float) frameDelays[i]) / 100;
			totalGifDuration += thisDurationSeconds;
			frameDelaysSeconds.push_back(thisDurationSeconds );
		}
	}
	std::vector<float> getAllFrameDelaysSeconds() {
		return frameDelaysSeconds;
	}
	float getTotalGifDuration() {
		return totalGifDuration;
	}
};