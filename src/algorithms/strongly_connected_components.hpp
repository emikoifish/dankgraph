#ifndef VG_ALGORITHMS_STRONGLY_CONNECTED_COMPONENTS_HPP_INCLUDED
#define VG_ALGORITHMS_STRONGLY_CONNECTED_COMPONENTS_HPP_INCLUDED

#include <unordered_set>
#include <handlegraph/handle_graph.hpp>
#include "dfs.hpp"

namespace vg {
namespace algorithms {

using namespace std;
using namespace handlegraph;

/// Find all of the nodes with no edges on their left sides.
vector<unordered_set<handlegraph::id_t>> strongly_connected_components(const HandleGraph* g);
    
}
}

#endif
