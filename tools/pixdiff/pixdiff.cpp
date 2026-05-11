// pixdiff <a.png> <b.png>
// Loads both images as RGBA8 via stb_image, prints byte-diff percentage to
// stdout. If dimensions or channel counts differ, prints 100.0. Exit code is
// always 0; the caller compares the printed number against its threshold.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdio>
#include <cstdlib>

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

int main(int argc, char **argv) {
	if (argc != 3) {
		std::fprintf(stderr, "usage: pixdiff <a.png> <b.png>\n");
		std::printf("100.0\n");
		return 0;
	}

	int aw = 0, ah = 0, bw = 0, bh = 0;
	unsigned char *ap = nullptr;
	unsigned char *bp = nullptr;
	if (!load_rgba(argv[1], &aw, &ah, &ap) ||
	    !load_rgba(argv[2], &bw, &bh, &bp)) {
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

	const size_t total = (size_t)aw * (size_t)ah * 4;
	size_t diff = 0;
	for (size_t i = 0; i < total; ++i) {
		if (ap[i] != bp[i]) ++diff;
	}

	const double pct = total == 0 ? 0.0 : (double)diff * 100.0 / (double)total;
	std::printf("%.4f\n", pct);

	stbi_image_free(ap);
	stbi_image_free(bp);
	return 0;
}
