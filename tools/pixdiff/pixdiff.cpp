// pixdiff [--crop x,y,w,h] <a.png> <b.png>
// Loads both images as RGBA8 via stb_image, prints byte-diff percentage to
// stdout. If dimensions or channel counts differ, prints 100.0. With --crop,
// both images are cropped to the same rect before comparing (rect must fit
// within both images; otherwise prints 100.0). Exit code is always 0; the
// caller compares the printed number against its threshold.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int load_rgba(const char *path, int *w, int *h, unsigned char **out) {
	int n = 0;
	*out = stbi_load(path, w, h, &n, 4);
	if (!*out) {
		std::fprintf(stderr, "pixdiff: failed to load %s: %s\n", path,
		             stbi_failure_reason());
		return 0;
	}
	return 1;
}

static int parse_crop(const char *s, int *x, int *y, int *w, int *h) {
	return std::sscanf(s, "%d,%d,%d,%d", x, y, w, h) == 4;
}

int main(int argc, char **argv) {
	int cx = 0, cy = 0, cw = -1, ch = -1;
	bool have_crop = false;
	int argi = 1;
	if (argi < argc && std::strcmp(argv[argi], "--crop") == 0) {
		if (argi + 1 >= argc || !parse_crop(argv[argi + 1], &cx, &cy, &cw, &ch)) {
			std::fprintf(stderr, "pixdiff: --crop expects x,y,w,h\n");
			std::printf("100.0\n");
			return 0;
		}
		have_crop = true;
		argi += 2;
	}

	if (argc - argi != 2) {
		std::fprintf(stderr,
		             "usage: pixdiff [--crop x,y,w,h] <a.png> <b.png>\n");
		std::printf("100.0\n");
		return 0;
	}

	int aw = 0, ah = 0, bw = 0, bh = 0;
	unsigned char *ap = nullptr;
	unsigned char *bp = nullptr;
	if (!load_rgba(argv[argi], &aw, &ah, &ap) ||
	    !load_rgba(argv[argi + 1], &bw, &bh, &bp)) {
		std::printf("100.0\n");
		if (ap) stbi_image_free(ap);
		if (bp) stbi_image_free(bp);
		return 0;
	}

	if (aw != bw || ah != bh) {
		std::printf("100.0\n");
		stbi_image_free(ap);
		stbi_image_free(bp);
		return 0;
	}

	int rx = 0, ry = 0, rw = aw, rh = ah;
	if (have_crop) {
		if (cx < 0 || cy < 0 || cw <= 0 || ch <= 0 || cx + cw > aw ||
		    cy + ch > ah) {
			std::fprintf(stderr,
			             "pixdiff: crop %d,%d,%d,%d out of bounds for %dx%d\n",
			             cx, cy, cw, ch, aw, ah);
			std::printf("100.0\n");
			stbi_image_free(ap);
			stbi_image_free(bp);
			return 0;
		}
		rx = cx;
		ry = cy;
		rw = cw;
		rh = ch;
	}

	size_t diff = 0;
	for (int y = 0; y < rh; ++y) {
		const unsigned char *arow = ap + ((size_t)(ry + y) * aw + rx) * 4;
		const unsigned char *brow = bp + ((size_t)(ry + y) * bw + rx) * 4;
		for (int i = 0; i < rw * 4; ++i) {
			if (arow[i] != brow[i]) ++diff;
		}
	}

	const size_t total = (size_t)rw * (size_t)rh * 4;
	const double pct = total == 0 ? 0.0 : (double)diff * 100.0 / (double)total;
	std::printf("%.4f\n", pct);

	stbi_image_free(ap);
	stbi_image_free(bp);
	return 0;
}
