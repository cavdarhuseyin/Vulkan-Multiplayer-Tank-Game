#include "keyboard_movement_controller.hpp"
#include <limits>
#include <cmath>
#include <glm/gtc/constants.hpp>

#include "lve_collision.hpp"

#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace lve {

    namespace {

        glm::vec3 getTurretForward(float yaw) {
            glm::vec3 forward{ -std::sin(yaw), 0.f, -std::cos(yaw) };
            return glm::normalize(forward);
        }

        // Kamera/tank gövdesi hareketi için mevcut ileri yön
        glm::vec3 getBodyForward(float yaw) {
            glm::vec3 forward{ std::sin(yaw), 0.f, std::cos(yaw) };
            return glm::normalize(forward);
        }

    } 

    void KeyboardMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, LveGameObject& gameObject) {
        glm::vec3 rotate{ 0.f };

        if (keys.lookRight != -1 && glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
        if (keys.lookLeft != -1 && glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
        if (keys.lookUp != -1 && glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
        if (keys.lookDown != -1 && glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
        }

        gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
        gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

        float yaw = gameObject.transform.rotation.y;
        const glm::vec3 forwardDir = getBodyForward(yaw);
        const glm::vec3 rightDir{ forwardDir.z, 0.f, -forwardDir.x };
        const glm::vec3 upDir{ 0.f, -1.f, 0.f };

        glm::vec3 moveDir{ 0.f };
        if (keys.moveForward != -1 && glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
        if (keys.moveBackward != -1 && glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
        if (keys.moveRight != -1 && glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
        if (keys.moveLeft != -1 && glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
        if (keys.moveUp != -1 && glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
        if (keys.moveDown != -1 && glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }

    void KeyboardMovementController::moveTankGamepad(
        float dt,
        LveGameObject& tankBody,
        LveGameObject& tankTurret,
        LveGameObject& missile
    ) {
        if (!glfwJoystickIsGamepad(GLFW_JOYSTICK_1)) return;

        GLFWgamepadstate state{};
        if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) return;

        auto applyDeadzone = [](float v, float dz) -> float {
            if (std::fabs(v) < dz) return 0.f;
            return v;
            };

        const float DZ = 0.18f;

        // Left stick: body move + body turn
        float moveForward = applyDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], DZ);
        float turnBody = applyDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], DZ);
        float turretTurn = applyDeadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], DZ);

        // -------- BODY ROTATION --------
        tankBody.transform.rotation.y += turnBody * lookSpeed * dt;
        tankBody.transform.rotation.y = glm::mod(tankBody.transform.rotation.y, glm::two_pi<float>());

        // -------- BODY MOVE --------
        float bodyYaw = tankBody.transform.rotation.y;
        glm::vec3 bodyForward = getBodyForward(bodyYaw);
        tankBody.transform.translation += bodyForward * moveForward * moveSpeed * dt;

        // -------- TURRET FOLLOW + ROTATION --------
        currentTurretAngle += turretTurn * turretTurnSpeed * dt;
        float totalTurretYaw = tankBody.transform.rotation.y + currentTurretAngle;

        tankTurret.transform.translation = tankBody.transform.translation;
        tankTurret.transform.rotation = tankBody.transform.rotation;
        tankTurret.transform.rotation.y = totalTurretYaw;

        // -------- SINGLE FORWARD VECTOR --------
        const float barrelLength = 2.50f;
        glm::vec3 muzzleOffsetLocal{ 0.0f, -1.0f, 0.0f };

        glm::vec3 forward = getTurretForward(totalTurretYaw);

        glm::vec3 muzzlePos =
            tankBody.transform.translation +
            muzzleOffsetLocal +
            forward * barrelLength;

        // -------- BUTTONS --------
        bool fireDown = (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS);
        bool reloadDown = (state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS);

        if (reloadDown && !reloadWasDownGp) {
            isMissileFired = false;
            missileVelocity = glm::vec3(0.f);
            missile.isActive = true;
        }
        reloadWasDownGp = reloadDown;

        if (fireDown && !fireWasDownGp && !isMissileFired) {
            PlaySound(TEXT("media\\tank_fire.wav"), NULL, SND_FILENAME | SND_ASYNC);

            isMissileFired = true;
            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;

            missileVelocity = forward * missileSpeed;
        }
        fireWasDownGp = fireDown;

        if (!isMissileFired) {
            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;
        }
        else {
            missile.transform.translation += missileVelocity * dt;
        }
    }

    void KeyboardMovementController::setupForTank() {
        keys.moveForward = GLFW_KEY_H;
        keys.moveBackward = GLFW_KEY_Y;
        keys.lookLeft = GLFW_KEY_G;
        keys.lookRight = GLFW_KEY_J;

        keys.turretLeft = GLFW_KEY_O;
        keys.turretRight = GLFW_KEY_I;

        keys.fire = GLFW_KEY_SPACE;
        keys.reload = GLFW_KEY_R;

        keys.moveLeft = -1;
        keys.moveRight = -1;
        keys.moveUp = -1;
        keys.moveDown = -1;
        keys.lookUp = -1;
        keys.lookDown = -1;

        moveSpeed = 3.5f;
        lookSpeed = 2.5f;
        turretTurnSpeed = 1.8f;
        missileSpeed = 30.0f;
    }

    void KeyboardMovementController::moveTank(
        GLFWwindow* window,
        float dt,
        LveGameObject& tankBody,
        LveGameObject& tankTurret,
        LveGameObject& missile,
        LveGameObject& sight,
        LveGameObject::Map& gameObjects
    ) {
        // -------- BODY ROTATE --------
        glm::vec3 rotate{ 0.f };
        if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
        if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            tankBody.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
        }
        tankBody.transform.rotation.y = glm::mod(tankBody.transform.rotation.y, glm::two_pi<float>());

        // -------- BODY MOVE WITH COLLISION --------
        float bodyYaw = tankBody.transform.rotation.y;
        glm::vec3 bodyForward = getBodyForward(bodyYaw);

        glm::vec3 rayOrigin = tankBody.transform.translation;
        rayOrigin.y -= 1.0f;

        glm::vec3 moveDir{ 0.f };
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += bodyForward;
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= bodyForward;

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            glm::vec3 rayDir = glm::normalize(moveDir);
            float closestHit = std::numeric_limits<float>::max();

            for (auto& kv : gameObjects) {
                auto& obj = kv.second;

                if (!obj.isActive ||
                    obj.model == nullptr ||
                    obj.getId() == tankBody.getId() ||
                    obj.getId() == tankTurret.getId() ||
                    obj.getId() == missile.getId() ||
                    obj.getId() == sight.getId()) {
                    continue;
                }

                float hitDistance = LveCollision::IntersectModel(
                    rayOrigin,
                    rayDir,
                    *obj.model,
                    obj.transform.mat4()
                );

                if (hitDistance < closestHit) {
                    closestHit = hitDistance;
                }
            }

            if (closestHit >= 2.5f) {
                tankBody.transform.translation += moveSpeed * dt * rayDir;
            }
        }

        // -------- TURRET ROTATION --------
        if (glfwGetKey(window, keys.turretLeft) == GLFW_PRESS) currentTurretAngle += turretTurnSpeed * dt;
        if (glfwGetKey(window, keys.turretRight) == GLFW_PRESS) currentTurretAngle -= turretTurnSpeed * dt;

        float totalTurretYaw = tankBody.transform.rotation.y + currentTurretAngle;

        // -------- TURRET FOLLOW --------
        glm::vec3 turretOffset{ 0.0f, 0.0f, 0.0f };

        tankTurret.transform.translation = tankBody.transform.translation + turretOffset;
        tankTurret.transform.rotation = tankBody.transform.rotation;
        tankTurret.transform.rotation.y = totalTurretYaw;

        // -------- RELOAD --------
        bool fireDown = (glfwGetKey(window, keys.fire) == GLFW_PRESS);
        bool reloadDown = (glfwGetKey(window, keys.reload) == GLFW_PRESS);

        if (reloadDown && !reloadWasDown) {
            isMissileFired = false;
            missileVelocity = glm::vec3(0.f);
            missile.isActive = true;
        }
        reloadWasDown = reloadDown;

        // -------- SINGLE FORWARD VECTOR --------
        const float barrelLength = 2.50f;
        glm::vec3 muzzleOffsetLocal{ 0.0f, -1.0f, 0.0f };

        glm::vec3 forward = getTurretForward(totalTurretYaw);

        glm::vec3 muzzlePos =
            tankBody.transform.translation +
            muzzleOffsetLocal +
            forward * barrelLength;

        // -------- SIGHT --------
        float sightClosestHit = std::numeric_limits<float>::max();

        for (auto& kv : gameObjects) {
            auto& obj = kv.second;

            if (!obj.isActive ||
                obj.model == nullptr ||
                obj.getId() == tankBody.getId() ||
                obj.getId() == tankTurret.getId() ||
                obj.getId() == missile.getId() ||
                obj.getId() == sight.getId()) {
                continue;
            }

            float hitDistance = LveCollision::IntersectModel(
                muzzlePos,
                forward,
                *obj.model,
                obj.transform.mat4()
            );

            if (hitDistance < sightClosestHit) {
                sightClosestHit = hitDistance;
            }
        }

        if (sightClosestHit < 1000.f) {
            sight.transform.translation = muzzlePos + (forward * sightClosestHit);
            sight.transform.translation -= forward * 0.05f;
        }
        else {
            sight.transform.translation = muzzlePos + (forward * 200.f);
        }

       
        sight.transform.rotation = tankTurret.transform.rotation;
        sight.transform.rotation.y += glm::pi<float>();

        // -------- FIRE --------
        if (fireDown && !fireWasDown && !isMissileFired) {
            PlaySound(TEXT("media\\tank_fire.wav"), NULL, SND_FILENAME | SND_ASYNC);

            isMissileFired = true;
            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;

            missileVelocity = forward * missileSpeed;
        }
        fireWasDown = fireDown;

        // -------- MISSILE --------
        if (!isMissileFired) {
            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;
        }
        else {
            glm::vec3 missileRayDir = glm::normalize(missileVelocity);
            float missileClosestHit = std::numeric_limits<float>::max();

            for (auto& kv : gameObjects) {
                auto& obj = kv.second;

                if (!obj.isActive ||
                    obj.model == nullptr ||
                    obj.getId() == tankBody.getId() ||
                    obj.getId() == tankTurret.getId() ||
                    obj.getId() == missile.getId() ||
                    obj.getId() == sight.getId()) {
                    continue;
                }

                float hitDistance = LveCollision::IntersectModel(
                    missile.transform.translation,
                    missileRayDir,
                    *obj.model,
                    obj.transform.mat4()
                );

                if (hitDistance < missileClosestHit) {
                    missileClosestHit = hitDistance;
                }
            }

            if (missileClosestHit < 0.5f) {
                missileVelocity = glm::vec3(0.f);
                missile.isActive = false;
            }
            else {
                missile.transform.translation += missileVelocity * dt;
            }
        }
    }

} // namespace lve