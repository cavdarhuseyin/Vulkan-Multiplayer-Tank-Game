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
    // Camera / hareketleri
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

    // Tank turret tuþlarý
    int turretLeft = GLFW_KEY_I;
    int turretRight = GLFW_KEY_O;

    // Ateþ etme / reload
    int fire = GLFW_KEY_SPACE;
    int reload = GLFW_KEY_R;
  };

  void moveTankGamepad(
      float dt,
      LveGameObject& tankBody,
      LveGameObject& tankTurret,
      LveGameObject& missile
  );


  // Standart camera hareketi
  void moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameObject);

  // Tank + turret + missile control
  void moveTank(GLFWwindow *window, float dt, LveGameObject &tankBody, LveGameObject &tankTurret,
                LveGameObject &missile, LveGameObject& sight,  LveGameObject::Map& gameObjects);

  // Tank tuþ atamalarý
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

  // edge detection için önceki tuþ durumlarý
  bool fireWasDown{false};
  bool reloadWasDown{false};
};

} 
