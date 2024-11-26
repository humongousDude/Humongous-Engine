#include <logger.hpp>
#include <material.hpp>
#include <model.hpp>

namespace Humongous
{
BoundingBox BoundingBox::GetAABB(glm::mat4 m)
{
    // HGINFO("PRE:::: Min: %f, %f, %f\n Max: %f, %f, %f\n", this->min.x, this->min.y, this->min.z, this->max.x, this->max.y, this->max.z);
    glm::vec3 min = glm::vec3(m[3]);
    glm::vec3 max = min;
    glm::vec3 v0, v1;

    glm::vec3 right = glm::vec3(m[0]);
    v0 = right * this->min.x;
    v1 = right * this->max.x;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    glm::vec3 up = glm::vec3(m[1]);
    v0 = up * this->min.y;
    v1 = up * this->max.y;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    glm::vec3 back = glm::vec3(m[2]);
    v0 = back * this->min.z;
    v1 = back * this->max.z;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    // HGINFO("POST:::: Min: %f, %f, %f\n Max: %f, %f, %f\n", this->min.x, this->min.y, this->min.z, this->max.x, this->max.y, this->max.z);
    return BoundingBox(min, max);
}
} // namespace Humongous
