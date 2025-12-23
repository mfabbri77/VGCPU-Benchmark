#include "doctest.h"
#include "vgcpu/artifacts/naming.hpp"
#include "vgcpu/artifacts/png_reader.hpp"
#include "vgcpu/artifacts/png_writer.hpp"
#include "vgcpu/artifacts/ssim_compare.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

using namespace vgcpu::artifacts;

TEST_CASE("Artifact Naming") {
    CHECK(generate_artifact_path("Tiger", "Skia", ".png") == "tiger_skia.png");
    CHECK(generate_artifact_path("Scene With Spaces", "B@ckend!", ".png") ==
          "scene_with_spaces_b_ckend_.png");
    CHECK(generate_artifact_path("UPPER", "lower", "") == "upper_lower");
}

TEST_CASE("PNG Writer & SSIM") {
    // 1. Create a dummy RGBA buffer (32x32 red)
    int w = 32, h = 32;
    std::vector<uint8_t> red_img(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        red_img[i * 4 + 0] = 255;  // R
        red_img[i * 4 + 1] = 0;    // G
        red_img[i * 4 + 2] = 0;    // B
        red_img[i * 4 + 3] = 255;  // A
    }

    // 2. Write to disk
    std::string filename = "test_red.png";
    bool wrote = write_png(filename, w, h, red_img);
    CHECK(wrote);
    CHECK(std::filesystem::exists(filename));

    // 2b. Read back
    int rw, rh;
    auto read_back = read_image(filename, rw, rh);
    CHECK(!read_back.empty());
    CHECK(rw == w);
    CHECK(rh == h);
    // Compare buffers (allow for minor compression diffs if any, but PNG should be lossless)
    CHECK(read_back.size() == red_img.size());
    // Sample check to avoid strict full buffer comparison if needed, but for simple red it should
    // match
    CHECK(read_back[0] == 255);
    CHECK(read_back[1] == 0);

    // 3. Compare with itself (perfect score)
    auto result_self = compute_ssim(w, h, red_img, w * 4, red_img, w * 4);
    CHECK(result_self.score == 1.0);
    CHECK(result_self.passed);

    // 4. Create a blue image
    std::vector<uint8_t> blue_img(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        blue_img[i * 4 + 0] = 0;
        blue_img[i * 4 + 1] = 0;
        blue_img[i * 4 + 2] = 255;
        blue_img[i * 4 + 3] = 255;
    }

    // 5. Compare red vs blue (should be low score)
    auto result_diff = compute_ssim(w, h, red_img, w * 4, blue_img, w * 4);
    CHECK(result_diff.score < 0.8);
    CHECK(!result_diff.passed);

    // Cleanup
    std::filesystem::remove(filename);
}
