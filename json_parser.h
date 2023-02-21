#pragma once

// https://github.com/aliakseis/json_parser

#include <any>
#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <memory>
#include <istream>

// Result consists of std::map<std::string, std::any>, 
// std::vector<std::any>, std::string, double and bool.
// Throws std::runtime_error on failure.
std::any parseJson(std::istream& input, bool skipError = false);
std::any parseJson(const std::string_view& input, bool skipError = false);
