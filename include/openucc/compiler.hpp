#pragma once
#include <functional>

#include "package.hpp"
#include "target.hpp"
namespace openucc {
// Explicit package-object declarations; requires a complete tagged-only
// reference.
Package compileObjects(
    const std::vector<std::pair<std::string, Bytes>>& sources,
    const TargetModel& target, const std::string& packageName,
    const std::function<void(const std::string&)>& progress = {});
// Bootstrap subset, intentionally rejects every unimplemented construct.
Package compile(const std::vector<std::pair<std::string, Bytes>>& sources,
                const Package& core);
Package compile(const std::vector<std::pair<std::string, Bytes>>& sources,
                const TargetModel& target, const std::string& packageName = {},
                const std::function<void(const std::string&)>& progress = {});
}  // namespace openucc
