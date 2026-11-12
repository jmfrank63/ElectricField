#pragma once
// The user's custom shapes (ef/shapes.hpp), one file "Shapes.ini" in the user directory:
//
//   [shape "Dumbbell"]
//   ball = 0, 1, 0        # centre of a ball relative to the shape's centre, in units of the ball radius
//   ball = 0, -1, 0
//
// A section needs a non-empty name that is not a built-in shape's, and must form a valid shape
// (ef::validShape: 1 to 32 balls that hang together); anything else is skipped.
#include <filesystem>
#include <string>
#include <vector>

#include "ef/shapes.hpp"

namespace efapp {

std::string serializeShapes(const std::vector<ef::Shape>& shapes);
std::vector<ef::Shape> parseShapes(const std::string& text);
std::vector<ef::Shape> loadShapes(const std::filesystem::path& file);
bool saveShapes(const std::filesystem::path& file, const std::vector<ef::Shape>& shapes, std::string& error);

// Index into builtin + custom (built-ins first), by name, case-insensitive; -1 when absent.
int findShape(const std::vector<ef::Shape>& custom, const std::string& name);
// The shape at that combined index.
const ef::Shape& shapeAt(const std::vector<ef::Shape>& custom, int index);

enum class SaveShapeResult { OK, EMPTY_NAME, RESERVED_NAME, INVALID_SHAPE };
// Adds `shape` to the custom list, or replaces the custom shape of the same name. The names of the
// built-in shapes are reserved.
SaveShapeResult upsertShape(std::vector<ef::Shape>& custom, const ef::Shape& shape);

}  // namespace efapp
