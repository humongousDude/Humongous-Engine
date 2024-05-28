#include "globals.hpp"
#include <keyboard_handler.hpp>

namespace Humongous
{
void KeyboardHandler::ProcessInput(const InputData& inputData)
{
    glm::vec3 rotate{0};
    glm::vec3 moveDir{0.0f};
    rotate.x -= (inputData.mouseDeltaY * lookSpeed * Globals::Time::DeltaTime());
    rotate.y += (inputData.mouseDeltaX * lookSpeed * Globals::Time::DeltaTime());

    if(glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
    {
        inputData.gameObject.transform.rotation += inputData.frameTime * rotate;
    }

    inputData.gameObject.transform.rotation.x = glm::clamp(inputData.gameObject.transform.rotation.x, -1.5f, 1.5f);
    inputData.gameObject.transform.rotation.y = glm::mod(inputData.gameObject.transform.rotation.y, glm::two_pi<float>());

    float yaw = inputData.gameObject.transform.rotation.y;
    float pitch = -inputData.gameObject.transform.rotation.x; // Assuming pitch is stored in x

    const glm::vec3 forwardDir(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw));

    const glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, glm::vec3(0.0f, -1.0f, 0.0f)));
    const glm::vec3 upDir = glm::cross(rightDir, forwardDir);

    switch(inputData.movementType)
    {
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
            moveDir += upDir;
            break;
        case Movements::DOWN:
            moveDir -= upDir;
            break;
        default:
            break;
    }

    if(glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
    {
        inputData.gameObject.transform.translation += moveSpeed * inputData.frameTime * glm::normalize(moveDir);
    }
}
}; // namespace Humongous
