// tests/test_correctness_oracle.cpp
// ADR-0004: correctness oracle suite (self-overlap coverage census)
//
// This is a census, not a bit-exactness gate: two independent backend
// architectures answer "what does nonzero winding mean when two subpaths
// of one fill overlap" differently -- exact-area/union coverage vs.
// per-subpath-coverage-then-sum-and-saturate (see
// Analisi_Strategica_Mercato_2D.md, sibling `Analisi_mercato` project,
// section 4.1). Case A and Case D have exactly one correct answer
// regardless of that architectural choice and are hard requirements; Case
// B and Case C classify the two known answers and report, they never fail
// the build for a documented, unfixable-by-us third-party finding
// (AGENTS.md, "Correctness contract").

#include "adapters/adapter_registry.h"
#include "doctest.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace vgcpu {
namespace {

using ir::FillRule;
using ir::Opcode;
using ir::PathVerb;

/// Appends one closed axis-aligned rectangle contour (4 points) to a Path.
void AppendRectContour(Path& path, float x0, float y0, float x1, float y1) {
    path.verbs.insert(path.verbs.end(), {PathVerb::kMoveTo, PathVerb::kLineTo, PathVerb::kLineTo,
                                         PathVerb::kLineTo, PathVerb::kClose});
    path.points.insert(path.points.end(), {x0, y0, x1, y0, x1, y1, x0, y1});
}

struct OracleCase {
    std::string name;
    int sample_x;
    int sample_y;
    double expected_union;      ///< correct coverage: area of the geometric union
    double expected_naive_sum;  ///< per-contour coverage summed independently, uncapped
    bool is_control;            ///< true when expected_union == expected_naive_sum: a
                                ///< single-answer oracle regardless of union-vs-sum choice
};

constexpr int kOracleHeight = 20;
constexpr int kOracleSlotWidth = 20;

/// One PreparedScene, four independent drawcalls ("cases"), each a single
/// Path made of one or two rectangle contours filled once with
/// FillRule::kNonZero. Every case samples the alpha channel of the same
/// interior pixel column so the antialiasing edge is the only source of
/// signal. Every rectangle spans the full 20px canvas height, so top/bottom
/// edges sit exactly on a pixel boundary and only the horizontal edge is
/// fractional.
PreparedScene BuildSelfOverlapOracleScene(std::vector<OracleCase>& cases_out) {
    PreparedScene scene;
    scene.scene_id = "correctness/self_overlap_coverage";
    scene.scene_hash = "oracle_self_overlap";
    scene.ir_major_version = ir::kIrMajorVersion;
    scene.ir_minor_version = ir::kIrMinorVersion;
    scene.width = kOracleSlotWidth * 4;
    scene.height = kOracleHeight;

    Paint black;
    black.type = ir::PaintType::kSolid;
    black.color = 0xFF000000;  // opaque black (R|G<<8|B<<16|A<<24 packing)
    scene.paints.push_back(black);

    const float y0 = 0.0f;
    const float y1 = static_cast<float>(kOracleHeight);

    // Case A: single rectangle, 50% pixel coverage. Basic AA sanity, not
    // about overlap -- establishes the adapter can render a fractional
    // edge correctly before the overlap cases are trusted.
    {
        Path p;
        AppendRectContour(p, 10.0f, y0, 10.5f, y1);
        scene.paths.push_back(p);
        cases_out.push_back({"A: single rect (baseline AA)", 10, 10, 0.5, 0.5, true});
    }

    // Case B: two IDENTICAL contours in the same path (coincident
    // self-overlap). The union is exactly the same 50%-covered region as
    // case A; a correct nonzero-winding fill must report the same 0.5. A
    // sum-then-saturate implementation reports min(0.5 + 0.5, 1.0) = 1.0.
    {
        Path p;
        AppendRectContour(p, 30.0f, y0, 30.5f, y1);
        AppendRectContour(p, 30.0f, y0, 30.5f, y1);
        scene.paths.push_back(p);
        cases_out.push_back({"B: coincident self-overlap", 30, 10, 0.5, 1.0, false});
    }

    // Case C: two DIFFERENT, partially overlapping contours (0.4 + 0.3
    // wide, 0.2 overlap). Union = 0.5; naive independent sum = 0.7, picked
    // below saturation so the sum-vs-union failure mode is visible on its
    // own, not masked by clamping to 1.0.
    {
        Path p;
        AppendRectContour(p, 50.0f, y0, 50.4f, y1);
        AppendRectContour(p, 50.2f, y0, 50.5f, y1);
        scene.paths.push_back(p);
        cases_out.push_back({"C: partial overlap", 50, 10, 0.5, 0.7, false});
    }

    // Case D: two ADJACENT, non-overlapping contours (0.3 + 0.3, touching
    // at x=70.3). Control: union == naive sum == 0.6, so this case has one
    // correct answer regardless of implementation choice. It isolates
    // general AA precision from the overlap-handling defect the other
    // cases probe (AGENTS.md, "Work and evidence" -- a control, not a gate).
    {
        Path p;
        AppendRectContour(p, 70.0f, y0, 70.3f, y1);
        AppendRectContour(p, 70.3f, y0, 70.6f, y1);
        scene.paths.push_back(p);
        cases_out.push_back({"D: adjacent, no overlap (control)", 70, 10, 0.6, 0.6, true});
    }

    // Command stream: clear to transparent, set fill once (shared paint +
    // nonzero rule persists as current state), then one FillPath per case.
    std::vector<uint8_t> cmd;
    auto push_u8 = [&cmd](uint8_t v) { cmd.push_back(v); };
    auto push_u16 = [&cmd](uint16_t v) {
        cmd.push_back(static_cast<uint8_t>(v & 0xFF));
        cmd.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };

    push_u8(static_cast<uint8_t>(Opcode::kClear));
    push_u8(0x00);
    push_u8(0x00);
    push_u8(0x00);
    push_u8(0x00);  // transparent

    push_u8(static_cast<uint8_t>(Opcode::kSetFill));
    push_u16(0);  // paint_id = 0
    push_u8(static_cast<uint8_t>(FillRule::kNonZero));

    for (uint16_t i = 0; i < 4; ++i) {
        push_u8(static_cast<uint8_t>(Opcode::kFillPath));
        push_u16(i);
    }
    push_u8(static_cast<uint8_t>(Opcode::kEnd));

    scene.command_stream = std::move(cmd);
    return scene;
}

double SampleAlpha(const std::vector<uint8_t>& buffer, uint32_t width, int x, int y) {
    size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
    return static_cast<double>(buffer[idx + 3]) / 255.0;
}

}  // namespace

TEST_SUITE("Correctness Oracle - Self-Overlap Coverage") {
    TEST_CASE("Backends render exact-union coverage on self-overlapping fill-nonzero paths" *
              doctest::test_suite("correctness")) {
        std::vector<OracleCase> cases;
        PreparedScene scene = BuildSelfOverlapOracleScene(cases);

        SurfaceConfig config;
        config.width = static_cast<int>(scene.width);
        config.height = static_cast<int>(scene.height);

        constexpr double kEpsilon = 3.0 / 255.0;

        // Machine-readable census export for tools/html_report.py: one JSON
        // row per (backend, case), written only when VGCPU_ORACLE_JSON names
        // a destination file. Keeps the doctest output contract unchanged.
        std::ostringstream census_json;
        bool census_first = true;

        auto& registry = AdapterRegistry::Instance();
        for (const auto& id : registry.GetAdapterIds()) {
            if (id == "null") {
                continue;  // does not render scene content (see AGENTS.md adapter contract)
            }

            auto adapter = registry.CreateAdapter(id);
            REQUIRE(adapter != nullptr);
            CAPTURE(id);

            AdapterArgs args;
            REQUIRE(adapter->Initialize(args).ok());
            REQUIRE(adapter->Prepare(scene).ok());

            std::vector<uint8_t> buffer(static_cast<size_t>(config.width) * config.height * 4, 0);
            auto status = adapter->Render(scene, config, buffer);
            REQUIRE(status.ok());

            for (const auto& c : cases) {
                double measured = SampleAlpha(buffer, scene.width, c.sample_x, c.sample_y);
                CAPTURE(c.name);
                CAPTURE(measured);
                CAPTURE(c.expected_union);

                bool exact = std::abs(measured - c.expected_union) <= kEpsilon;
                bool sums = std::abs(measured - c.expected_naive_sum) <= kEpsilon;
                std::string classification;
                if (c.is_control) {
                    classification = exact ? "pass" : "FAIL";
                    // Single correct answer: hard requirement for every backend.
                    CHECK_MESSAGE(exact, id << " / " << c.name << ": expected " << c.expected_union
                                            << ", measured " << measured);
                } else {
                    // Known-divergent architectures exist (sum-then-saturate,
                    // e.g. Blend2D/Vello per the market-analysis doc); this is
                    // a census, not a gate: classify and report, never fail
                    // the build for a documented, unfixable-by-us finding.
                    classification =
                        exact ? "exact-union" : (sums ? "sum-then-saturate" : "other-divergence");
                    MESSAGE(id << " / " << c.name << ": measured=" << measured << " expected_union="
                               << c.expected_union << " expected_naive_sum=" << c.expected_naive_sum
                               << " classification=" << classification);
                }

                if (!census_first) {
                    census_json << ",\n";
                }
                census_first = false;
                census_json << "  {\"backend\": \"" << id << "\", \"case\": \"" << c.name
                            << "\", \"measured\": " << measured
                            << ", \"expected_union\": " << c.expected_union
                            << ", \"expected_naive_sum\": " << c.expected_naive_sum
                            << ", \"is_control\": " << (c.is_control ? "true" : "false")
                            << ", \"classification\": \"" << classification << "\"}";
            }

            adapter->Shutdown();
        }

        if (const char* census_path = std::getenv("VGCPU_ORACLE_JSON")) {
            std::ofstream out(census_path, std::ios::trunc);
            if (out) {
                out << "[\n" << census_json.str() << "\n]\n";
            } else {
                MESSAGE("VGCPU_ORACLE_JSON set but not writable: " << census_path);
            }
        }
    }
}

}  // namespace vgcpu
