// 本文件声明独立弹体的封闭网格碰撞；不依赖任何游戏状态。
#pragma once
#include "raylib.h"
#include <filesystem>
#include <vector>
#include <string>
namespace review {
struct Triangle { Vector3 a{}, b{}, c{}; };
struct Bounds { Vector3 lo{}, hi{}; };
struct Node { Bounds box{}; int left=-1,right=-1,begin=0,count=0; };
struct Part { std::vector<Triangle> triangles; std::vector<int> order; std::vector<Node> nodes; };
/// @brief 每个查询线程独占并复用的交点工作区；Prepare 在加载/预热阶段调用。
struct CollisionScratch {
    std::vector<float> crossings;
    void Prepare(std::size_t maxTriangles) { crossings.reserve(maxTriangles); }
};
/// @brief 各闭合部件取实体并集。单位为模型本地长度；无缩放、无资源借用。
class CollisionAsset {
public:
    /// @brief 读取 KCOL0001 小端浮点格式；非法或空资产抛 runtime_error，不创建 GPU 资源。
    explicit CollisionAsset(const std::filesystem::path& path);
    /// @brief 本地点到实体并集的距离平方；内部为 0，真实孔洞为空。
    float DistanceSquared(Vector3 point) const;
    /// @brief 无逐次分配版本；scratch 容量至少为 MaxPartTriangles()，不足时抛异常。
    float DistanceSquared(Vector3 point,CollisionScratch& scratch) const;
    /// @brief 模型可平移、旋转和正的统一缩放；非法/非统一缩放抛异常。返回世界长度平方。
    float WorldDistanceSquared(Vector3 point, Matrix model) const;
    /// @brief 与本地球形探针接触，radius 必须非负；距离比较包含表面。
    bool Touches(Vector3 point,float radius) const;
    const std::vector<Part>& Parts() const { return parts_; }
    int TriangleCount() const;
    std::size_t MaxPartTriangles() const;
    /// @brief 以模型原点为中心的保守包围球半径，仅用于初筛，不替代实体判定。
    float BoundingRadius() const { return bound_; }
private:
    std::vector<Part> parts_;
    float bound_=0;
};
} // namespace review
