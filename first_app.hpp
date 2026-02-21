#pragma once

#include "lve_descriptors.hpp"
#include "lve_window.hpp"
#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_renderer.hpp"

#include <memory>
#include <vector>

namespace lve {

class FirstApp {
public:
  static constexpr int WIDTH = 800;
  static constexpr int HEIGHT = 600;

  FirstApp();
  ~FirstApp();

  FirstApp(const FirstApp &) = delete;
  FirstApp &operator=(const FirstApp &) = delete;

  void run();

private:
  void loadGameObjects();

  LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
  LveDevice lveDevice{lveWindow};
  LveRenderer lveRenderer{lveWindow, lveDevice};

  std::unique_ptr<LveDescriptorPool> globalPool{};
  LveGameObject::Map gameObjects;

  // Object IDs
  LveGameObject::id_t tankId{};
  LveGameObject::id_t turretId{};
  LveGameObject::id_t missileId{};
  LveGameObject::id_t townId{};
  LveGameObject::id_t groundId{};
};

} // namespace lve
