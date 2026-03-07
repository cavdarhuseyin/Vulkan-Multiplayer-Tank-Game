#pragma once

#include "lve_game_object.hpp"
#include "lve_window.hpp"

namespace lve {

class KeyboardMovementController {

private:
    bool fireWasDownGp{ false };
    bool reloadWasDownGp{ false };

public:
  struct KeyMappings {
    // Camera / generic movement keys (default)
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int moveForward = GLFW_KEY_W;
    int moveBackward = GLFW_KEY_S;
    int moveUp = GLFW_KEY_E;
    int moveDown = GLFW_KEY_Q;
    int lookLeft = GLFW_KEY_LEFT;
    int lookRight = GLFW_KEY_RIGHT;
    int lookUp = GLFW_KEY_UP;
    int lookDown = GLFW_KEY_DOWN;

    // Tank turret keys
    int turretLeft = GLFW_KEY_I;
    int turretRight = GLFW_KEY_O;

    // Fire / reload
    int fire = GLFW_KEY_SPACE;
    int reload = GLFW_KEY_R;
  };

  void moveTankGamepad(
      float dt,
      LveGameObject& tankBody,
      LveGameObject& tankTurret,
      LveGameObject& missile
  );


  // Standard camera movement
  void moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameObject);

  // Tank + turret + missile control
  void moveTank(GLFWwindow *window, float dt, LveGameObject &tankBody, LveGameObject &tankTurret,
                LveGameObject &missile, LveGameObject& sight,  LveGameObject::Map& gameObjects);

  // Tank key bindings setup
  void setupForTank();

  KeyMappings keys{};

  float moveSpeed{3.f};
  float lookSpeed{1.5f};
  float turretTurnSpeed{1.5f};
  float missileSpeed{25.0f};

private:
  float currentTurretAngle{0.0f};
  bool isMissileFired{false};
  glm::vec3 missileVelocity{0.f};

  // edge-trigger (so fire/reload happens once per key press)
  bool fireWasDown{false};
  bool reloadWasDown{false};
};

} // namespace lve
