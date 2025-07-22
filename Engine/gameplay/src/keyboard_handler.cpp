#include "keyboard_handler.hpp"
#include "extra.hpp"
#include "globals.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif

namespace Humongous
{

void KeyboardHandler::ProcessInput(const InputData& inputData)
{
    Eigen::Vector3f moveDir = Eigen::Vector3f::Zero();

    Eigen::Vector3f currentEulerAngles = inputData.camera.GetRotation();
    static float    accumulatedPitch = currentEulerAngles.x();
    static float    accumulatedYaw = currentEulerAngles.y();

    accumulatedYaw += (inputData.mouseDeltaX * lookSpeed * static_cast<float>(Globals::Time::AverageDeltaTime()));
    accumulatedPitch += (inputData.mouseDeltaY * lookSpeed * static_cast<float>(Globals::Time::AverageDeltaTime()));

    accumulatedPitch = std::clamp(accumulatedPitch, Utils::DegreesToRadians(-89.0f), Utils::DegreesToRadians(89.0f));
    accumulatedYaw = std::fmod(accumulatedYaw, 2.0f * EIGEN_PI);

    inputData.camera.SetRotation(Eigen::Vector3f(accumulatedPitch, accumulatedYaw, 0.0f));

    Eigen::Vector3f forwardBase = Eigen::Vector3f(0.0f, 0.0f, 1.0f);
    Eigen::Vector3f rightBase = Eigen::Vector3f(1.0f, 0.0f, 0.0f);
    Eigen::Vector3f upBase = Eigen::Vector3f(0.0f, 1.0f, 0.0f);

    Eigen::Quaternionf cameraOrientation =
        Eigen::AngleAxisf(accumulatedYaw, Eigen::Vector3f::UnitY()) * Eigen::AngleAxisf(accumulatedPitch, Eigen::Vector3f::UnitX());
    cameraOrientation.normalize();

    Eigen::Vector3f forwardDir = (cameraOrientation * forwardBase).normalized();
    Eigen::Vector3f rightDir = (cameraOrientation * rightBase).normalized();
    Eigen::Vector3f upDir = Eigen::Vector3f::UnitY();

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

    if(moveDir.squaredNorm() > 0.001f)
    {
        moveDir.normalize();
        inputData.camera.SetPosition(inputData.camera.GetPosition() + moveSpeed * inputData.frameTime * moveDir);
    }

    inputData.camera.UpdateViewMatrix();
}

}; // namespace Humongous
