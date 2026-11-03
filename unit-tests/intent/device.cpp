// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
// Explicit hardware test: requires an exact serial and an opt-in mode.
#include <chrono>
#include <fstream>
#include <iostream>
#include <librealsense2/hpp/rs_intent.hpp>
#include <librealsense2/rs.hpp>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
using json = nlohmann::json;
static void require(bool ok, const std::string &why) {
  if (!ok)
    throw std::runtime_error(why);
}
static void delay(unsigned ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
static json declaration(unsigned mask, bool dual, bool yuyv = false) {
  json stereo = json::array(), color = json::array();
  auto stream = [](const char *b, const char *f) {
    return json{{"branch", b},
                {"format", f},
                {"width", 1280},
                {"height", 720},
                {"fps", 30}};
  };
  if (mask & 1)
    stereo.push_back(stream("DEPTH", "Z16"));
  if (mask & 2)
    stereo.push_back(stream("IR_LR", "Y8I"));
  if (dual) {
    if (mask & 4)
      stereo.push_back(stream("RGB_L", "NV12"));
    if (mask & 8)
      stereo.push_back(stream("RGB_R", "NV12"));
  } else if (mask & 4)
    color.push_back(stream("RGB", yuyv ? "YUY2" : "NV12"));
  json sensors = json::array();
  if (!stereo.empty())
    sensors.push_back({{"sensor", "STEREO"}, {"streams", stereo}});
  if (!color.empty())
    sensors.push_back({{"sensor", "COLOR"}, {"streams", color}});
  return {{"version", {{"major", 1}, {"minor", 0}}}, {"sensors", sensors}};
}
struct Group {
  rs2::sensor sensor;
  std::vector<rs2::stream_profile> profiles;
  bool opened = false, started = false;
};
struct Counts {
  std::mutex mutex;
  std::map<std::string, unsigned> frames;
  std::map<std::string, double> last;
  unsigned bad = 0;
  bool checking = true;
  void accept(rs2::frame f) {
    auto p = f.get_profile();
    auto v = f.as<rs2::video_frame>();
    if (!v)
      return;
    std::string key = std::to_string(p.stream_type()) + ":" +
                      std::to_string(p.stream_index());
    std::lock_guard<std::mutex> lock(mutex);
    if (checking && (v.get_width() != 1280 || v.get_height() != 720 ||
                     f.get_data_size() == 0))
      ++bad;
    if (frames[key] && f.get_timestamp() < last[key])
      ++bad;
    last[key] = f.get_timestamp();
    ++frames[key];
  }
  json snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    return {{"frames", frames}, {"bad_frames", bad}};
  }
  unsigned total() {
    std::lock_guard<std::mutex> lock(mutex);
    unsigned n = 0;
    for (auto &v : frames)
      n += v.second;
    return n;
  }
};
static std::vector<Group> groups(rs2::device dev, unsigned mask, bool dual,
                                 bool left_ir, bool yuyv,
                                 bool right_ir = false) {
  struct Want {
    rs2_stream type;
    int index;
    rs2_format format;
  };
  std::vector<Want> wants;
  if (mask & 1)
    wants.push_back({RS2_STREAM_DEPTH, 0, RS2_FORMAT_Z16});
  if (mask & 2) {
    if (!right_ir)
      wants.push_back({RS2_STREAM_INFRARED, 1, RS2_FORMAT_Y8});
    if (!left_ir)
      wants.push_back({RS2_STREAM_INFRARED, 2, RS2_FORMAT_Y8});
  }
  if (mask & 4)
    wants.push_back({RS2_STREAM_COLOR, dual ? 1 : 0,
                     yuyv ? RS2_FORMAT_YUYV : RS2_FORMAT_NV12});
  if (dual && (mask & 8))
    wants.push_back({RS2_STREAM_COLOR, 2, RS2_FORMAT_NV12});
  std::vector<Group> result;
  for (auto sensor : dev.query_sensors()) {
    Group g{sensor, {}};
    for (auto it = wants.begin(); it != wants.end();) {
      bool found = false;
      for (auto p : sensor.get_stream_profiles()) {
        auto v = p.as<rs2::video_stream_profile>();
        if (v && p.stream_type() == it->type && p.stream_index() == it->index &&
            p.format() == it->format && p.fps() == 30 && v.width() == 1280 &&
            v.height() == 720) {
          g.profiles.push_back(p);
          found = true;
          break;
        }
      }
      if (found)
        it = wants.erase(it);
      else
        ++it;
    }
    if (!g.profiles.empty())
      result.push_back(g);
  }
  require(wants.empty(), "Required SDK profile not advertised");
  return result;
}
static void stop(std::vector<Group> &gs) {
  for (auto it = gs.rbegin(); it != gs.rend(); ++it) {
    if (it->started) {
      it->sensor.stop();
      it->started = false;
    }
    if (it->opened) {
      it->sensor.close();
      it->opened = false;
    }
  }
}
static void rejected(rs2::intent_device &intent, json request,
                     const char *name) {
  bool threw = false;
  try {
    intent.set_intent(request.dump());
  } catch (const rs2::error &) {
    threw = true;
  }
  require(threw, std::string("Expected rejection: ") + name);
  std::cout << json{{"case", name},
                    {"rejected", true},
                    {"status", json::parse(intent.get_intent())["status"]}}
                   .dump()
            << std::endl;
}
static void run_set(rs2::device dev, unsigned mask, bool dual, bool left_ir,
                    bool yuyv, bool legacy = false, bool right_ir = false) {
  rs2::intent_device intent(dev);
  auto doc = declaration(mask, dual, yuyv);
  if (!legacy) {
    intent.set_intent(doc.dump());
    require(json::parse(intent.get_intent())["status"]["state"] == 2,
            "SET did not configure");
  }
  auto gs = groups(dev, mask, dual, left_ir, yuyv, right_ir);
  Counts counts;
  try {
    for (auto &g : gs) {
      g.sensor.open(g.profiles);
      g.opened = true;
    }
    // V4L2 open() already issues STREAMON (UVC COMMIT plus START). SDK
    // start() enables callbacks; it is not the firmware START boundary.
    for (auto &g : gs) {
      g.sensor.start([&](rs2::frame f) { counts.accept(f); });
      g.started = true;
    }
    unsigned needed = 0;
    for (auto &g : gs)
      needed += g.profiles.size();
    for (int i = 0; i < 100; ++i) {
      delay(100);
      auto s = counts.snapshot();
      bool enough = s["frames"].size() == needed;
      for (auto &v : s["frames"].items())
        enough &= v.value().get<unsigned>() >= 20;
      if (enough)
        break;
    }
    auto snap = counts.snapshot();
    require(snap["bad_frames"] == 0,
            "Bad dimensions/empty frame/timestamp regression");
    require(snap["frames"].size() == needed, "Missing requested stream");
    for (auto &v : snap["frames"].items())
      require(v.value().get<unsigned>() >= 20, "Insufficient frames");
    if (!legacy) {
      auto st = json::parse(intent.get_intent())["status"];
      require(st["state"] == 4, "Not LOCKED while running");
      unsigned expected = (mask & 1 ? (1U << 1) : 0U) |
                          (mask & 2 ? (1U << 4) : 0U) |
                          (mask & 4 ? (1U << (dual ? 5 : 7)) : 0U) |
                          (dual && (mask & 8) ? (1U << 6) : 0U);
      require(st["active_mask"] == expected,
              "Active mask does not match native streams");
      rejected(intent, {{"op", "CLEAR"}}, "CLEAR while active");
      rejected(intent, doc, "SET while active");
    }
    // In 3C, stop the dedicated RGB sensor and verify the stereo producer
    // continues.
    bool continuity = false;
    if (gs.size() > 1) {
      gs.back().sensor.stop();
      gs.back().started = false;
      gs.back().sensor.close();
      gs.back().opened = false;
      delay(200);
      auto before = counts.total();
      delay(600);
      require(counts.total() > before + 5, "Peer stopped when RGB stopped");
      continuity = true;
      if (!legacy) {
        auto st = json::parse(intent.get_intent())["status"];
        require(st["state"] == 4 &&
                    (st["active_mask"].get<unsigned>() & (1U << 7)) == 0,
                "Color close did not detach its native gate");
      }
      const auto stopped = counts.snapshot()["frames"];
      gs.back().sensor.open(gs.back().profiles);
      gs.back().opened = true;
      gs.back().sensor.start([&](rs2::frame f) { counts.accept(f); });
      gs.back().started = true;
      bool resumed = false;
      for (unsigned i = 0; i < 30 && !resumed; ++i) {
        delay(100);
        const auto after = counts.snapshot()["frames"];
        resumed = true;
        for (auto p : gs.back().profiles) {
          const auto key = std::to_string(p.stream_type()) + ":" +
                           std::to_string(p.stream_index());
          resumed &=
              after.contains(key) &&
              after[key].get<unsigned>() >= stopped[key].get<unsigned>() + 5;
        }
      }
      require(resumed, "Color failed to resume after reopen");
      require(counts.snapshot()["bad_frames"] == 0,
              "Invalid frame after peer restart");
    }
    stop(gs);
    if (!legacy) {
      auto st = json::parse(intent.get_intent())["status"];
      require(st["state"] == 2 && st["active_mask"] == 0,
              "Final STOP did not retain idle Intent");
    }
    std::cout << json{{"case", legacy ? "legacy" : "stream-set"},
                      {"mask", mask},
                      {"dual", dual},
                      {"left_ir", left_ir},
                      {"right_ir", right_ir},
                      {"yuyv", yuyv},
                      {"peer_continuity", continuity},
                      {"capture", snap},
                      {"pass", true}}
                     .dump()
              << std::endl;
  } catch (...) {
    std::cerr << json{{"mask", mask}, {"capture", counts.snapshot()}}.dump()
              << std::endl;
    if (!legacy) {
      try {
        std::cerr << intent.get_intent() << std::endl;
      } catch (...) {
      }
    }
    try {
      stop(gs);
    } catch (...) {
    }
    throw;
  }
}
int main(int argc, char **argv) {
  try {
    require(
        argc >= 3,
        "Usage: intent-device SERIAL "
        "status|matrix|left-ir|right-ir|yuyv|negative|guard|pipeline|legacy|"
        "switch-2c|switch-3c [mask]");
    rs2::context ctx;
    rs2::device dev;
    for (auto d : ctx.query_devices())
      if (d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) == std::string(argv[1]))
        dev = d;
    require(bool(dev), "Exact target serial not found");
    const std::string mode = argv[2],
                      pid = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
    const bool dual = pid == "0C07";
    rs2::intent_device intent(dev);
    std::cout << json{{"device", dev.get_info(RS2_CAMERA_INFO_NAME)},
                      {"serial", argv[1]},
                      {"pid", pid},
                      {"fw", dev.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION)},
                      {"usb",
                       dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR)}}
                     .dump()
              << std::endl;
    if (mode == "status") {
      std::cout << intent.get_intent() << std::endl;
      return 0;
    }
    if (mode == "switch-2c" || mode == "switch-3c") {
      try {
        intent.set_intent(R"({"op":"CLEAR"})");
      } catch (...) {
      }
      bool set = false;
      for (auto s : dev.query_sensors())
        if (s.supports(RS2_OPTION_SENSORS_CONFIG_MODE)) {
          s.set_option(RS2_OPTION_SENSORS_CONFIG_MODE,
                       mode == "switch-2c" ? 1.f : 0.f);
          set = true;
          break;
        }
      require(set, "Topology switch not exposed");
      dev.hardware_reset();
      std::cout
          << "Reset requested; rediscover the new identity before further tests"
          << std::endl;
      return 0;
    }
    require(std::stof(dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR)) >= 3.f,
            "USB3 required by pilot Intent matrix");
    if (mode == "legacy") {
      run_set(dev, argc > 3 ? std::stoul(argv[3]) : 1, dual, false, false,
              true);
      return 0;
    }
    intent.set_intent(R"({"op":"CLEAR"})");
    if (mode == "negative") {
      auto doc = declaration(1, dual);
      intent.set_intent(doc.dump());
      auto before = json::parse(intent.get_intent())["sensors"];
      auto bad = doc;
      bad["sensors"][0]["streams"][0]["fps"] = 15;
      rejected(intent, bad, "unsupported FPS");
      require(json::parse(intent.get_intent())["sensors"] == before,
              "Rejected SET changed declaration");
      auto validate = doc;
      validate["op"] = "VALIDATE_ONLY";
      intent.set_intent(validate.dump());
      require(json::parse(intent.get_intent())["sensors"] == before,
              "VALIDATE_ONLY changed declaration");
      auto wrong = groups(dev, 2, dual, false, false);
      bool failed = false;
      try {
        wrong[0].sensor.open(wrong[0].profiles);
        wrong[0].opened = true;
      } catch (const rs2::error &) {
        failed = true;
      }
      stop(wrong);
      require(failed, "Undeclared IR COMMIT accepted");
      std::cout << json{{"case", "negative"}, {"pass", true}}.dump()
                << std::endl;
    } else if (mode == "guard") {
      intent.set_intent(declaration(5, dual).dump());
      auto gs = groups(dev, 4, dual, false, false);
      Counts counts;
      try {
        for (auto &g : gs) {
          g.sensor.open(g.profiles);
          g.opened = true;
          g.sensor.start([&](rs2::frame f) { counts.accept(f); });
          g.started = true;
        }
        delay(1000);
        require(counts.total() > 5,
                "No color frames before control guard test");
        bool checked = false;
        for (auto sensor : dev.query_sensors())
          for (auto filter : sensor.query_embedded_filters()) {
            if (!filter.supports_composite_option(
                    RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP))
              continue;
            auto before = filter.get_composite_option(
                RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP);
            bool failed = false;
            try {
              filter.set_composite_option(
                  RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP, before.data(),
                  before.size());
            } catch (const rs2::error &) {
              failed = true;
            }
            require(
                failed,
                "Decimation write accepted with gated internal Depth running");
            require(filter.get_composite_option(
                        RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP) == before,
                    "Rejected control changed XU cache");
            checked = true;
          }
        require(checked, "Decimation control not exposed");
        stop(gs);
        std::cout << json{{"case", "gated-depth control protection"},
                          {"capture", counts.snapshot()},
                          {"pass", true}}
                         .dump()
                  << std::endl;
      } catch (...) {
        try {
          stop(gs);
        } catch (...) {
        }
        throw;
      }
    } else if (mode == "pipeline") {
      intent.set_intent(declaration(5, dual).dump());
      rs2::pipeline pipeline(ctx);
      rs2::config cfg;
      cfg.enable_device(argv[1]);
      cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720, RS2_FORMAT_Z16, 30);
      cfg.enable_stream(RS2_STREAM_COLOR, dual ? 1 : 0, 1280, 720,
                        RS2_FORMAT_RGB8, 30);
      pipeline.start(cfg);
      unsigned complete = 0;
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(10);
      while (complete < 30 && std::chrono::steady_clock::now() < deadline) {
        auto f = pipeline.wait_for_frames(1000);
        auto color = f.get_color_frame(dual ? 1 : 0);
        if (f.get_depth_frame() && color) {
          require(color.get_profile().format() == RS2_FORMAT_RGB8 &&
                      color.get_width() == 1280 && color.get_height() == 720,
                  "Unexpected converted color frame");
          ++complete;
        }
      }
      pipeline.stop();
      require(complete >= 30, "Insufficient complete Depth/Color framesets");
      std::cout
          << json{{"case", "pipeline RGB8→native NV12"}, {"pass", true}}.dump()
          << std::endl;
    } else {
      require(mode == "matrix" || mode == "left-ir" || mode == "right-ir" ||
                  mode == "yuyv",
              "Unknown mode");
      unsigned max = dual ? 15 : 7;
      unsigned first = argc > 3 ? std::stoul(argv[3]) : 1;
      unsigned last = argc > 3 ? first : max;
      for (unsigned mask = first; mask <= last; ++mask) {
        if ((mode == "left-ir" || mode == "right-ir") && !(mask & 2))
          continue;
        if (mode == "yuyv" && (!(mask & 4) || dual))
          continue;
        run_set(dev, mask, dual, mode == "left-ir", mode == "yuyv", false,
                mode == "right-ir");
      }
    }
    intent.set_intent(R"({"op":"CLEAR"})");
    require(json::parse(intent.get_intent())["status"]["state"] == 0,
            "CLEAR failed");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << std::endl;
    return 1;
  }
}
