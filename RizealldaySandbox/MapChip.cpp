#include "MapChip.h"
#include <RizeEngine.h>
#include <fstream>
#include <sstream>
#include <cmath>

bool MapChip::IsFlyingSpawn(const Vector3& pos) {
	for (const auto& p : sFlyingSpawnPositions_) {
		if (std::abs(p.x - pos.x) < 0.05f && std::abs(p.y - pos.y) < 0.05f) {
			return true;
		}
	}
	return false;
}

void MapChip::LoadCSV(const std::string& filePath, Model* cubeModel, Camera* camera)
{
	tiles_.clear();

	std::ifstream file(filePath);
	if (!file.is_open())
	{
		return;
	}

	std::vector<std::vector<int>> mapGrid;
	std::string line;
	bool isFirstLine = true;

	while (std::getline(file, line))
	{
		if (line.empty()) continue;

		// Strip UTF-8 BOM (0xEF, 0xBB, 0xBF) if present at start of first line
		if (isFirstLine)
		{
			isFirstLine = false;
			if (line.size() >= 3 &&
				static_cast<unsigned char>(line[0]) == 0xEF &&
				static_cast<unsigned char>(line[1]) == 0xBB &&
				static_cast<unsigned char>(line[2]) == 0xBF)
			{
				line.erase(0, 3);
			}
		}

		if (line.empty()) continue;
		std::vector<int> rowValues;
		std::stringstream ss(line);
		std::string cell;

		while (std::getline(ss, cell, ','))
		{
			while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ' || cell.back() == '\t'))
			{
				cell.pop_back();
			}
			size_t start = cell.find_first_not_of(" \t");
			if (start != std::string::npos)
			{
				cell = cell.substr(start);
			}
			else
			{
				cell.clear();
			}

			if (!cell.empty())
			{
				try
				{
					rowValues.push_back(std::stoi(cell));
				}
				catch (...)
				{
					// Ignore any non-integer or malformed cell safely
				}
			}
		}
		if (!rowValues.empty())
		{
			mapGrid.push_back(rowValues);
		}
	}

	if (mapGrid.empty()) return;

	int totalRows = static_cast<int>(mapGrid.size());
	int totalCols = static_cast<int>(mapGrid[0].size());
	totalRows_ = totalRows;
	totalCols_ = totalCols;

	playerSpawnPos_ = { 0.5f, 0.0f, 0.0f };
	enemySpawnPositions_.clear();
	flyingEnemySpawnPositions_.clear();
	sFlyingSpawnPositions_.clear();

	for (int row = 0; row < totalRows; ++row) {
		for (int col = 0; col < totalCols; ++col) {
			int chip = mapGrid[row][col];
			if (chip == 0) continue; // 0 = empty air

			// 2 = Player Spawn
			if (chip == 2) {
				playerSpawnPos_ = {
					col * tileSize_,
					(totalRows - 1 - row) * tileSize_ - tileSize_,
					0.0f
				};
				continue;
			}

			// 3 = Ground Enemy Spawn
			if (chip == 3) {
				Vector3 enemySpawnPos = {
					col * tileSize_,
					(totalRows - 1 - row) * tileSize_ - tileSize_,
					0.0f
				};
				enemySpawnPositions_.push_back(enemySpawnPos);
				continue;
			}

			// 9 = Flying Enemy Spawn
			if (chip == 9) {
				Vector3 flySpawnPos = {
					col * tileSize_,
					(totalRows - 1 - row) * tileSize_ - tileSize_,
					0.0f
				};
				enemySpawnPositions_.push_back(flySpawnPos);
				flyingEnemySpawnPositions_.push_back(flySpawnPos);
				sFlyingSpawnPositions_.push_back(flySpawnPos);
				continue;
			}

			auto isSolidCell = [&](int r, int c) {
				if (r < 0 || r >= totalRows || c < 0 || c >= totalCols) return false;
				int ch = mapGrid[r][c];
				return (ch != 0 && ch != 2 && ch != 3 && ch != 9);
			};

			TileBlock block;
			block.chipId = chip;
			block.hasTop = !isSolidCell(row - 1, col);
			block.hasBottom = !isSolidCell(row + 1, col);
			block.hasLeft = !isSolidCell(row, col - 1);
			block.hasRight = !isSolidCell(row, col + 1);

			Vector3 blockPos = {
				col * tileSize_,
				(totalRows - 1 - row) * tileSize_ - (tileSize_ * 0.5f),
				0.0f
			};

			// Defaults
			block.isDestructible = false;
			block.isDestroyed = false;
			block.slopeType = 0;
			block.isMoving = false;
			block.moveType = 0;
			block.startPos = blockPos;
			block.endPos = blockPos;
			block.moveSpeed = 1.8f;
			block.moveTimer = static_cast<float>(row % 10) * 0.5f;
			block.moveDistance = 2.0f;

			Vector3 modelScale = Vector3(tileSize_ * 0.5f, tileSize_ * 0.5f, 0.5f);
			Vector3 modelRot = Vector3(0.0f, 0.0f, 0.0f);
			Vector4 matColor = { 0.80f, 0.77f, 0.70f, 1.0f }; // Default light ink wash
			// Paper & ink: brush outline only on the sides that are not covered by another tile
			int32_t inkEdgeMask = 16
				| (block.hasTop ? 1 : 0) | (block.hasBottom ? 2 : 0)
				| (block.hasLeft ? 4 : 0) | (block.hasRight ? 8 : 0);

			if (chip == 1) {
				// Solid blocks (Tembok, Platform, Lantai)
				bool isBorder = (row == 0 || col == 0 || col == totalCols - 1);
				bool isCeilingOrWall = isBorder || (!block.hasTop && (block.hasLeft || block.hasRight));
				bool isFloatingPlatform = block.hasTop && block.hasBottom;

				if (isFloatingPlatform) {
					// Floating Platform (mid sumi wash)
					matColor = { 0.66f, 0.62f, 0.56f, 1.0f };
				}
				else if (isCeilingOrWall) {
					// Tembok / Wall / Border (dense sumi ink wash)
					matColor = { 0.42f, 0.40f, 0.39f, 1.0f };
				}
				else {
					// Tanah / Solid Ground (pale wash)
					matColor = { 0.80f, 0.77f, 0.70f, 1.0f };
				}
			}
			else if (chip == 4) {
				// Slanted Block Right (/)
				block.slopeType = 1;
				matColor = { 0.48f, 0.58f, 0.50f, 1.0f }; // Muted bamboo ink
				modelRot.z = -0.785398f;
				inkEdgeMask = 31;
			}
			else if (chip == 5) {
				// Slanted Block Left (\)
				block.slopeType = 2;
				matColor = { 0.48f, 0.58f, 0.50f, 1.0f }; // Muted bamboo ink
				modelRot.z = 0.785398f;
				inkEdgeMask = 31;
			}
			else if (chip == 6) {
				// Destructible Block
				block.isDestructible = true;
				matColor = { 0.82f, 0.36f, 0.26f, 1.0f }; // Vermilion hanko-stamp red
			}
			else if (chip == 7) {
				// Moving Platform (Horizontal)
				block.isMoving = true;
				block.moveType = 1;
				block.moveSpeed = 1.8f;
				block.moveDistance = 2.5f;
				matColor = { 0.36f, 0.42f, 0.56f, 1.0f }; // Indigo ink
				inkEdgeMask = 31;
				modelScale.y = tileSize_ * 0.35f; // Slim slab
				blockPos.y += (tileSize_ * 0.5f - modelScale.y); // Flush top
				block.startPos = blockPos;
				block.endPos = blockPos + Vector3(block.moveDistance, 0.0f, 0.0f);
			}
			else if (chip == 8) {
				// Moving Platform (Vertical)
				block.isMoving = true;
				block.moveType = 2;
				block.moveSpeed = 1.5f;
				block.moveDistance = 3.0f;
				matColor = { 0.36f, 0.42f, 0.56f, 1.0f }; // Indigo ink
				inkEdgeMask = 31;
				modelScale.y = tileSize_ * 0.35f; // Slim slab
				blockPos.y += (tileSize_ * 0.5f - modelScale.y); // Flush top
				block.startPos = blockPos;
				block.endPos = blockPos + Vector3(0.0f, block.moveDistance, 0.0f);
			}

			// Setup Collision AABB
			block.aabb.SetCenterSize(blockPos, modelScale);
			block.aabb.attribute = (chip == 1) ? CollisionAttribute::kGround : CollisionAttribute::kObstacle;
			block.aabb.mask = CollisionAttribute::kPlayer | CollisionAttribute::kBall | CollisionAttribute::kEnemy;
			block.aabb.enabled = true;

			// Setup Model 3D
			block.modelObj.Initialize();
			block.modelObj.SetModel(cubeModel);
			block.modelObj.SetCamera(camera);
			block.modelObj.worldTransform.translation = blockPos;
			block.modelObj.worldTransform.scale = modelScale;
			block.modelObj.worldTransform.rotation = modelRot;

			block.modelObj.GetMaterial()->color = matColor;
			block.modelObj.GetMaterial()->inkEdgeMask = inkEdgeMask;
			block.modelObj.GetMaterial()->enableLighting = 1;
			block.modelObj.GetMaterial()->lightingType = 2; // Half-Lambert paper lighting
			block.modelObj.Update(0.0f);
			tiles_.push_back(std::move(block));
		}
	}
}

void MapChip::Update(float deltaTime) {
	for (auto& tile : tiles_) {
		if (tile.isDestroyed) continue;

		if (tile.isMoving && deltaTime > 0.0f) {
			tile.moveTimer += deltaTime;
			float offset = std::sin(tile.moveTimer * tile.moveSpeed) * tile.moveDistance;
			Vector3 curPos = tile.startPos;
			if (tile.moveType == 1) {
				curPos.x += offset;
			}
			else if (tile.moveType == 2) {
				curPos.y += offset;
			}
			tile.deltaPos = curPos - tile.modelObj.worldTransform.translation;
			Vector3 halfExtent = tile.modelObj.worldTransform.scale;
			tile.aabb.SetCenterSize(curPos, halfExtent);
			tile.modelObj.worldTransform.translation = curPos;
		}

		tile.modelObj.Update(deltaTime);
	}
}

void MapChip::RegisterColliders(CollisionManager* colMgr) {
	for (auto& tile : tiles_) {
		if (tile.isDestroyed || !tile.aabb.enabled) continue;
		colMgr->RegisterCollider(&tile);
	}
}

void MapChip::Draw() {
	auto* cmdList = RizeEngine::GetDirectXCommon()->GetCommandList();
	for (auto& tile : tiles_) {
		if (tile.isDestroyed) continue;
		tile.modelObj.Draw(cmdList);
	}
}