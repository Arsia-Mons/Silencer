// One-shot fixture generator for pixdiff tests.
// Produces three 16x16 RGBA PNGs in the directory given as argv[1]:
//   identical_a.png, identical_b.png (same pixels)
//   totally_different.png            (every pixel inverted)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../clients/silencer/third_party/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int write_png(const std::string &path, const unsigned char *px) {
	const int w = 16;
	const int h = 16;
	if (!stbi_write_png(path.c_str(), w, h, 4, px, w * 4)) {
		std::fprintf(stderr, "gen_fixtures: failed to write %s\n", path.c_str());
		return 0;
	}
	return 1;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		std::fprintf(stderr, "usage: gen_fixtures <out_dir>\n");
		return 1;
	}
	const std::string dir = argv[1];

	const int w = 16;
	const int h = 16;
	unsigned char a[w * h * 4];
	unsigned char diff[w * h * 4];
	for (int i = 0; i < w * h; ++i) {
		a[i * 4 + 0] = 32;
		a[i * 4 + 1] = 96;
		a[i * 4 + 2] = 160;
		a[i * 4 + 3] = 255;
		diff[i * 4 + 0] = 223;
		diff[i * 4 + 1] = 159;
		diff[i * 4 + 2] = 95;
		diff[i * 4 + 3] = 255;
	}

	if (!write_png(dir + "/identical_a.png", a)) return 1;
	if (!write_png(dir + "/identical_b.png", a)) return 1;
	if (!write_png(dir + "/totally_different.png", diff)) return 1;

	std::printf("wrote 3 fixtures to %s\n", dir.c_str());
	return 0;
}
