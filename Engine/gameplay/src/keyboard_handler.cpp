#include "keyboard_handler.hpp"
#include "globals.hpp"

namespace Humongous
{
void KeyboardHandler::ProcessInput(const InputData& inputData)
{
    glm::vec3 moveDir{0.0f};

    // Get current rotation from camera
    glm::vec3    currentRotation = inputData.camera.GetRotation();
    static float accumulatedPitch = currentRotation.x;
    static float accumulatedYaw = currentRotation.y;

    accumulatedYaw -= (inputData.mouseDeltaX * lookSpeed * static_cast<float>(Globals::Time::AverageDeltaTime()));
    accumulatedPitch += (inputData.mouseDeltaY * lookSpeed * static_cast<float>(Globals::Time::AverageDeltaTime()));

    accumulatedPitch = glm::clamp(accumulatedPitch, -glm::radians(89.0f), glm::radians(89.0f));
    accumulatedYaw = glm::mod(accumulatedYaw, glm::two_pi<float>());

    inputData.camera.SetRotation(glm::vec3(accumulatedPitch, accumulatedYaw, 0.0f));

    glm::vec3 forwardDir =
        glm::vec3(0.0f, 0.0f, -1.0f); // Initial forward direction (along +Z in LH system with glm::perspectiveLH) - now corrected to +Z
    glm::vec3 rightDir = glm::vec3(1.0f, 0.0f, 0.0f); // Initial right direction (along +X) - remains the same
    glm::vec3 upDir = glm::vec3(0.0f, 1.0f, 0.0f);    // Initial up direction (along +Y) - remains the same

    glm::mat4 cameraRotationMatrix = glm::mat4(1.0f);
    cameraRotationMatrix = glm::rotate(cameraRotationMatrix, accumulatedYaw, glm::vec3(0.0f, 1.0f, 0.0f));
    cameraRotationMatrix = glm::rotate(cameraRotationMatrix, accumulatedPitch, glm::vec3(1.0f, 0.0f, 0.0f));

    forwardDir = glm::normalize(glm::vec3(cameraRotationMatrix * glm::vec4(forwardDir, 0.0f)));
    rightDir = glm::normalize(glm::vec3(cameraRotationMatrix * glm::vec4(rightDir, 0.0f)));
    upDir = glm::normalize(glm::vec3(cameraRotationMatrix * glm::vec4(upDir, 0.0f)));

    switch(inputData.movementType)
    {
        case Movements::NONE:
            break;
        case Movements::FORWARD:
            moveDir += forwardDir;
            break;
        case Movements::BACKWARD:
            moveDir -= forwardDir;
            break;
        case Movements::LEFT:
            moveDir -= rightDir;
            break;
        case Movements::RIGHT:
            moveDir += rightDir;
            break;
        case Movements::UP:
            moveDir -= upDir;
            break;
        case Movements::DOWN:
            moveDir += upDir;
            break;
        case Movements::FORWARD_LEFT:
            moveDir += forwardDir;
            moveDir -= rightDir;
            break;
        case Movements::FORWARD_RIGHT:
            moveDir += forwardDir;
            moveDir += rightDir;
            break;
        case Movements::BACKWARD_LEFT:
            moveDir -= forwardDir;
            moveDir -= rightDir;
            break;
        case Movements::BACKWARD_RIGHT:
            moveDir -= forwardDir;
            moveDir += rightDir;
            break;
        default:
            break;
    }

    if(glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
    {
        moveDir = glm::normalize(moveDir);
        // Move the camera's position, not a separate game object
        inputData.camera.SetPosition(inputData.camera.GetPosition() + moveSpeed * inputData.frameTime * moveDir);
    }

    // IMPORTANT: Update the view matrix AFTER any position or rotation changes!
    inputData.camera.UpdateViewMatrix();
}
}; // namespace Humongous
