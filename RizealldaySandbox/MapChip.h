#pragma once
#include "3d/CollisionManager.h"
#include "3d/Collider.h"
#include "3d/Object3d.h"
#include "math/Vector3.h"
#include <vector>
#include <string>

class TileBlock :public ICollidable
{
public:
	AABBCollider aabb{};
	Object3d modelObj;
	int chipId = 0; // 1 = ground, 2 = wall, 4 = slope R, 5 = slope L, 6 = destructible, 7 = moving H, 8 = moving V

	bool isDestructible = false;
	bool isDestroyed = false;
	int slopeType = 0; // 0 = None, 1 = Right (/), 2 = Left (\)

	// Moving platform variables
	bool isMoving = false;
	int moveType = 0; // 0 = None, 1 = Horizontal, 2 = Vertical
	Vector3 startPos = { 0.0f, 0.0f, 0.0f };
	Vector3 endPos = { 0.0f, 0.0f, 0.0f };
	Vector3 deltaPos = { 0.0f, 0.0f, 0.0f };
	float moveSpeed = 1.8f;
	float moveTimer = 0.0f;
	float moveDistance = 1.5f;

	// Exposed faces (true if neighbor in that direction is open air / passable)
	bool hasTop = true;
	bool hasBottom = true;
	bool hasLeft = true;
	bool hasRight = true;

	ColliderShape GetColliderShape() const override { return ColliderShape::AABB; }
	const AABBCollider& GetAABBCollider()const override { return aabb; }

};


class MapChip
{
public:
	void LoadCSV(const std::string& filePath, Model* cubeModel, Camera* camera);

	void Update(float deltaTime);
	void Draw();

	//Register all to CollisionManager
	void RegisterColliders(CollisionManager* colMgr);

	const Vector3& GetPlayerSpawnPos() const { return playerSpawnPos_; }
	const std::vector<Vector3>& GetEnemySpawnPositions() const { return enemySpawnPositions_; }
	const std::vector<Vector3>& GetFlyingEnemySpawnPositions() const { return flyingEnemySpawnPositions_; }
	static bool IsFlyingSpawn(const Vector3& pos);

	int GetTotalRows() const { return totalRows_; }
	int GetTotalCols() const { return totalCols_; }
	float GetTileSize() const { return tileSize_; }
	float GetWidth() const { return totalCols_ * tileSize_; }
	float GetHeight() const { return totalRows_ * tileSize_; }
	Vector3 GetCenter() const { return { totalCols_ * tileSize_ * 0.5f, totalRows_ * tileSize_ * 0.5f, 0.0f }; }
	const std::vector<TileBlock>& GetTiles() const { return tiles_; }

private:
	float tileSize_ = 0.5f;
	int totalRows_ = 0;
	int totalCols_ = 0;
	std::vector<TileBlock>tiles_;
	Vector3 playerSpawnPos_ = { 0.5f, 0.0f, 0.0f };
	std::vector<Vector3> enemySpawnPositions_;
	std::vector<Vector3> flyingEnemySpawnPositions_;
	static inline std::vector<Vector3> sFlyingSpawnPositions_;
};

