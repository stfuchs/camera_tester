/*
 * Copyright 2021 Fetch Robotics Inc.
 * Author: Steffen Fuchs
 */

// Std
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

// 3rd
/*
 * Intel RealSense API reference: https://intelrealsense.github.io/librealsense/doxygen/index.html
 */
#include <librealsense2/rs.hpp>


// Some helper functions for logging
std::string timestamp()
{
  using namespace std::chrono;
  using clock = high_resolution_clock;

  const auto time_point {clock::now()};
  const auto time {clock::to_time_t(time_point)};
  const auto duration {time_point.time_since_epoch()};
  const auto t_sec {duration_cast<seconds>(duration)};
  const auto t_msec {duration_cast<microseconds>(duration - t_sec)};

  std::ostringstream stream;
  stream << std::put_time(std::localtime(&time), "%F %T")
         << "." << std::setw(6) << std::setfill('0') << t_msec.count();
  return stream.str();
}

#define LOG(name, msg) { std::cout << timestamp() << " [" << name << "] " << msg << std::endl; }


// A minimalistic multi cam driver for testing
class MiniDriver
{
public:
  enum State
  {
    STATE_OFF=0,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_ERROR,
  };

  MiniDriver(const std::string& serial) : serial_(serial), interrupt_requested_(false), state_(STATE_OFF)
  {}

  bool is_running() { return state_ == STATE_RUNNING; }

  void start(bool block_until_ready=true)
  {
    stop();
    LOG(serial_, "Starting driver.");
    state_ = STATE_STARTING;
    thread_ = std::thread(std::bind(&MiniDriver::run, this));
    if (block_until_ready)
    {
      while (state_ == STATE_STARTING)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    LOG(serial_, "Started driver.");
  }

  void stop()
  {
    if (thread_.joinable())
    {
      interrupt_requested_ = true;
      thread_.join();
      interrupt_requested_ = false;
    }
  }

  void reset()
  {
    rs2::context ctx;
    for (auto&& dev : ctx.query_devices())
    {
      if (std::string(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER)) != serial_)
      {
        continue;
      }

      LOG(serial_, "Performing hardware reset.");
      dev.hardware_reset();
      LOG(serial_, "Reset triggered! Waiting for 5 seconds...");
      std::this_thread::sleep_for(std::chrono::seconds(5));
      LOG(serial_, "Reset complete!");
    }
  }

  void run()
  {
    try
    {
      reset();
      run_unsafe();
    }
    catch (const rs2::error& e)
    {
      state_ = STATE_ERROR;
      LOG(serial_, "librealsense error: " << e.get_failed_function() << " | " << e.get_failed_args());
      LOG(serial_, e.what());
    }
    catch (const std::runtime_error& e)
    {
      state_ = STATE_ERROR;
      LOG(serial_, "librealsense std::runtime_error: " << e.what());
    }
  }

  void run_unsafe()
  {
    rs2::config config;
    config.enable_device(serial_);
    config.enable_stream(RS2_STREAM_DEPTH, 0, 848, 480, RS2_FORMAT_Z16, 6);
    config.enable_stream(RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_ANY, 6);
    rs2::pipeline pipeline;
    rs2::pipeline_profile pipeline_prof = pipeline.start(config);

    rs2::device device = pipeline_prof.get_device();

    // print some info
    auto _get_info = [&device](rs2_camera_info info) {
      return std::string(device.supports(info) ? device.get_info(info) : "n/a");
    };

    LOG(serial_, "Name          : " + _get_info(RS2_CAMERA_INFO_NAME));
    LOG(serial_, "FW Update ID  : " + _get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID));
    LOG(serial_, "FW Version    : " + _get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION));
    LOG(serial_, "FW Recommended: " + _get_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION));
    LOG(serial_, "Product ID    : " + _get_info(RS2_CAMERA_INFO_PRODUCT_ID));
    LOG(serial_, "USB Type      : " + _get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR));
    LOG(serial_, "Physical Port : " + _get_info(RS2_CAMERA_INFO_PHYSICAL_PORT));

    state_ = STATE_RUNNING;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    TimePoint tp_started = Clock::now();
    TimePoint tp_last_logged = tp_started;
    int64_t last_frame_number = 0;
    int64_t frame_counter = 0;

    while (!interrupt_requested_)
    {
      rs2::frameset frames = pipeline.wait_for_frames(5000);

      const auto now = Clock::now();

      const Duration time_since_last_log{now - tp_last_logged};
      if (time_since_last_log.count() > 1) // Throttle log output
      {
        float fps = frame_counter / time_since_last_log.count();
        LOG(serial_, "FPS: " << fps << " (" << frame_counter << " / " << time_since_last_log.count() << ")");
        //if (fps < 3)
        //{
        //  throw std::runtime_error("FPS drop too much.");
        //}
        frame_counter = 0;
        tp_last_logged = now;
      }

      const auto fd = frames.get_depth_frame();

      if (!fd) continue;

      const int64_t new_frame_number = fd.get_frame_number();

      if (new_frame_number > last_frame_number)
      {
        ++frame_counter; // new valid depth frame
      }
      else if (new_frame_number < last_frame_number)
      {
        LOG(serial_, "Frame number reset: " << last_frame_number << " -> " << new_frame_number);
      }

      last_frame_number = new_frame_number;
    }

    pipeline.stop();
    state_ = STATE_OFF;
  }

  std::string serial_;
  std::thread thread_;
  std::atomic<bool> interrupt_requested_;
  std::atomic<State> state_;
};


int main(int argc, char** argv)
{
  std::vector<std::shared_ptr<MiniDriver>> drivers;

  rs2::context ctx;
  for (auto&& dev : ctx.query_devices())
  {
    drivers.push_back(std::make_shared<MiniDriver>(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER)));
  }

  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;
  using Duration = std::chrono::duration<double>;

  bool toggle = true;
  auto started = Clock::now();

  for (int i=0; i<4; ++i)
  {
    drivers[i]->start();
  }

  while (true)
  {
    const auto now = Clock::now();
    const Duration run_time{now - started};
    if (run_time.count() > 3600)
    {
      if (toggle)
      {
        for (int i=0; i<4; ++i)
        {
          drivers[i]->stop();
        }
        for (int i=4; i<8; ++i)
        {
          drivers[i]->start();
        }
      }
      else
      {
        for (int i=4; i<8; ++i)
        {
          drivers[i]->stop();
        }
        for (int i=0; i<4; ++i)
        {
          drivers[i]->start();
        }
      }

      toggle != toggle;
      started = now;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
