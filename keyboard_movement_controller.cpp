#include "keyboard_movement_controller.hpp"
#include <limits>
#include <glm/gtc/constants.hpp>

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace lve {

    void KeyboardMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, LveGameObject& gameObject) {
        glm::vec3 rotate{ 0 };
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
        const glm::vec3 forwardDir{ sin(yaw), 0.f, cos(yaw) };
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

        // 1) AXES (Deadzone + normalize)
        auto applyDeadzone = [](float v, float dz) -> float {
            if (fabs(v) < dz) return 0.f;
            return v;
            };

        const float DZ = 0.18f; // drift için biraz daha yüksek

        float moveForward = applyDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], DZ); 
        float turnBody = applyDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], DZ);
        float turretTurn = applyDeadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], DZ);

        // 2) BODY ROTATION
        tankBody.transform.rotation.y += turnBody * lookSpeed * dt;
        tankBody.transform.rotation.y = glm::mod(tankBody.transform.rotation.y, glm::two_pi<float>());

        // 3) BODY MOVE
        float bodyYaw = tankBody.transform.rotation.y;
        glm::vec3 bodyForward{ sin(bodyYaw), 0.f, cos(bodyYaw) };
        tankBody.transform.translation += bodyForward * moveForward * moveSpeed * dt;

        // 4) TURRET ROTATION + FOLLOW
        currentTurretAngle += turretTurn * turretTurnSpeed * dt;

        float totalTurretYaw = tankBody.transform.rotation.y + currentTurretAngle;

        tankTurret.transform.translation = tankBody.transform.translation; // offset varsa
        tankTurret.transform.rotation = tankBody.transform.rotation;
        tankTurret.transform.rotation.y = totalTurretYaw;

        // 5) MUZZLE POS
        const float barrelLength = -2.50f;                
        glm::vec3 muzzleOffsetLocal{ 0.0f, -1.0f, 0.0f };  

        glm::vec3 dir{ sin(totalTurretYaw), 0.f, cos(totalTurretYaw) };
        dir = glm::normalize(dir);

        glm::vec3 muzzlePos =
            tankBody.transform.translation +
            muzzleOffsetLocal +
            dir * barrelLength;

        // 6) BUTTONS (Edge trigger)
        bool fireDown = (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS);
        bool reloadDown = (state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS);

        // RELOAD edge
        if (reloadDown && !reloadWasDownGp) {
            isMissileFired = false;
            missileVelocity = glm::vec3(0.f);
        }
        reloadWasDownGp = reloadDown;

        // FIRE edge
        if (fireDown && !fireWasDownGp && !isMissileFired) {
            
            PlaySound(TEXT("media\\tank_fire.wav"), NULL, SND_FILENAME | SND_ASYNC);

            isMissileFired = true;

            missile.transform.translation = muzzlePos;               
            missile.transform.rotation = tankTurret.transform.rotation;

            missileVelocity = dir * missileSpeed;                   
        }
        fireWasDownGp = fireDown;

        // 7) MISSILE UPDATE
        if (!isMissileFired) {
            // ateþlenmediyse namluda dursun
            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;
        }
        else {
            // ateþlendiyse uçsun
            missile.transform.translation -= missileVelocity * dt;
        }
    }



    void KeyboardMovementController::setupForTank() {
        // Body movement
        keys.moveForward = GLFW_KEY_H;
        keys.moveBackward = GLFW_KEY_Y;
        keys.lookLeft = GLFW_KEY_G;
        keys.lookRight = GLFW_KEY_J;

        // Turret
        keys.turretLeft = GLFW_KEY_O;
        keys.turretRight = GLFW_KEY_I;

        // Fire / reload
        keys.fire = GLFW_KEY_SPACE;
        keys.reload = GLFW_KEY_R;

        // disable unused
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

    void KeyboardMovementController::moveTank(GLFWwindow* window, float dt,
        LveGameObject& tankBody,
        LveGameObject& tankTurret,
        LveGameObject& missile) {
        // -------- Body rotate --------
        glm::vec3 rotate{ 0 };
        if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
        if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            tankBody.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
        }
        tankBody.transform.rotation.y = glm::mod(tankBody.transform.rotation.y, glm::two_pi<float>());

        // -------- Body translate --------
        float bodyYaw = tankBody.transform.rotation.y;
        const glm::vec3 bodyForward{ sin(bodyYaw), 0.f, cos(bodyYaw) };

        glm::vec3 moveDir{ 0.f };
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += bodyForward;
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= bodyForward;

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            tankBody.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }

        // -------- Turret rotation (relative) --------
        if (glfwGetKey(window, keys.turretLeft) == GLFW_PRESS) currentTurretAngle += turretTurnSpeed * dt;
        if (glfwGetKey(window, keys.turretRight) == GLFW_PRESS) currentTurretAngle -= turretTurnSpeed * dt;

        float totalTurretYaw = tankBody.transform.rotation.y + currentTurretAngle;

        // -------- Turret follow body --------
        // Turret'ýn gövdeye göre offset'i (gerekirse ayarlanýr)
        glm::vec3 turretOffset{ 0.0f, 0.0f, 0.0f };

        tankTurret.transform.translation = tankBody.transform.translation + turretOffset;
        tankTurret.transform.rotation = tankBody.transform.rotation;
        tankTurret.transform.rotation.y = totalTurretYaw;

        // -------- Missile logic --------
        bool fireDown = (glfwGetKey(window, keys.fire) == GLFW_PRESS);
        bool reloadDown = (glfwGetKey(window, keys.reload) == GLFW_PRESS);

        // Edge trigger: reload
        if (reloadDown && !reloadWasDown) {
            isMissileFired = false;
            missileVelocity = glm::vec3(0.f);
        }
        reloadWasDown = reloadDown;

        // -------- MUZZLE (namlu ucu) hesabý --------

        const float barrelLength = -2.50f;

        // Merminin namlu ucunda durmasý için turret pivotundan küçük bir offset ver.
        glm::vec3 muzzleOffsetLocal{ 0.0f, -1.0f, 0.0f }; // yukarý/aþaðý için Y'yi oynat
        glm::vec3 dir{ sin(totalTurretYaw), 0.f, cos(totalTurretYaw) };
        dir = glm::normalize(dir);
        
        glm::vec3 muzzlePos =
            tankBody.transform.translation
            + muzzleOffsetLocal
            + dir * barrelLength;

        // Edge trigger: fire
        if (fireDown && !fireWasDown && !isMissileFired) {
           
            PlaySound(TEXT("media\\tank_fire.wav"), NULL, SND_FILENAME | SND_ASYNC);
            
            isMissileFired = true;

            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;

            missileVelocity = dir * missileSpeed;
        }
        fireWasDown = fireDown;

        if (!isMissileFired) {
            // Ateþlenmediyse namlu ucuna yapýþýk kalsýn
            missile.transform.translation = muzzlePos;
            missile.transform.rotation = tankTurret.transform.rotation;
        }
        else {
            // Ateþlendiyse ileri uçsun (DOÐRU YÖN)
            missile.transform.translation -= missileVelocity * dt;
        }
    }

} // namespace lve
