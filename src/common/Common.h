#pragma once

#include <string>

// Get absolute path to UNIX socket used for communication between engine and GUI
std::string getSocketPath();

constexpr auto GRAPH_FILE_SUFFIX = ".filtergraph";
