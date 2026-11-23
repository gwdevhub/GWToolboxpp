#pragma once

#include <glaze/glaze.hpp>

// The opts every glaze call uses. JSON is read and written a few times a session, so glaze's smallest code beats its
// fastest: linear key search instead of a hash table per struct, and small number tables. Results are the same.
struct SizeOptimisedJson : glz::opts {
    glz::optimization_level optimization_level = glz::optimization_level::size;
};
