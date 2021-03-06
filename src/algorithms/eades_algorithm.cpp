//#define debug_eades

#include "eades_algorithm.hpp"

namespace vg {
namespace algorithms {

using namespace std;
using namespace handlegraph;

    vector<handle_t> eades_algorithm(const HandleGraph* graph) {
        
#ifdef debug_eades
        cerr << "entering Eades algorithm" << endl;
#endif
        
        // decide which strand will be "forward" for each node
        vector<handle_t> canonical_orientation = single_stranded_orientation(graph);
        if (canonical_orientation.size() < graph->node_size()) {
            cerr << "error:[eades_algorithm] Eades' algorithm only valid on graphs with a single stranded orientation" << endl;
            exit(1);
        }
        
#ifdef debug_eades
        cerr << "got canonical orientation:" << endl;
        for (const handle_t& h : canonical_orientation) {
            cerr << "\t" << graph->get_id(h) << (graph->get_is_reverse(h) ? "-" : "+") << endl;
        }
#endif
        
        // maps handles to records of ((in-degree, out-degree), bucket position)
        unordered_map<handle_t, pair<pair<int64_t, int64_t>, list<handle_t>::iterator>> degree_info;
        
        // buckets based on delta(u) among non-source, non-sink nodes (see paper)
        // buckets are numbered -n + 2, -n + 3, ... , n - 3, n - 2
        auto assign_bucket = [&](const int64_t& in_degree, const int64_t& out_degree) {
            return out_degree - in_degree + int64_t(canonical_orientation.size()) - 2;
        };
        vector<list<handle_t>> delta_buckets(canonical_orientation.size() > 1 ? 2 * canonical_orientation.size() - 3 : 0);
        
        
        vector<handle_t> sources;
        vector<handle_t> sinks;
        
        for (const handle_t& handle : canonical_orientation) {
            // compute in- and out-degree
            int64_t in_degree = graph->get_degree(handle, true);
            int64_t out_degree = graph->get_degree(handle, false);
            
            if (in_degree == 0) {
                // source
                sources.emplace_back(handle);
#ifdef debug_eades
                cerr << "assign " << graph->get_id(handle) << (graph->get_is_reverse(handle) ? "-" : "+") << " to sources" << endl;
#endif
            }
            else if (out_degree == 0) {
                // sink
                sinks.emplace_back(handle);
#ifdef debug_eades
                cerr << "assign " << graph->get_id(handle) << (graph->get_is_reverse(handle) ? "-" : "+") << " to sinks" << endl;
#endif
            }
            else {
                // non-source, non-sink
                auto& bucket = delta_buckets[assign_bucket(in_degree, out_degree)];
                bucket.emplace_front(handle);
                degree_info[handle] = make_pair(make_pair(in_degree, out_degree), bucket.begin());
#ifdef debug_eades
                cerr << "assign " << graph->get_id(handle) << (graph->get_is_reverse(handle) ? "-" : "+") << " to delta bucket " << assign_bucket(in_degree, out_degree) << endl;
#endif
            }
        }
        
        // identify the highest non-empty bucket
        int64_t max_delta_bucket = delta_buckets.size() - 1;
        while (max_delta_bucket >= 0) {
            if (!delta_buckets[max_delta_bucket].empty()) {
                break;
            }
            max_delta_bucket--;
        }
        
        // init the layout to fill
        vector<handle_t> layout(canonical_orientation.size());
        
        // the next positions to add to in the layout (we fill from both sides)
        int64_t next_left_idx = 0;
        int64_t next_right_idx = layout.size() - 1;
        
        // update data structures to remove an edge into a node
        function<void(const handle_t&)> remove_inward_edge = [&](const handle_t& next) {
            auto iter = degree_info.find(next);
            if (iter != degree_info.end()) {
                // this node is in a delta bucket
                
                // remove it from the current bucket
                auto& degrees = iter->second.first;
                int64_t current_bucket_num = assign_bucket(degrees.first, degrees.second);
                auto& bucket = delta_buckets[current_bucket_num];
                bucket.erase(iter->second.second);
                
                // update the degrees to remove the inward edge
                degrees.first--;
                if (degrees.first == 0) {
                    // this is now a source
                    degree_info.erase(next);
                    sources.push_back(next);
                }
                else {
                    // this moves up one bucket
                    current_bucket_num++;
                    auto& new_bucket = delta_buckets[current_bucket_num];
                    new_bucket.emplace_front(next);
                    iter->second.second = new_bucket.begin();
                    
                    // if necessary, identify this as the new highest delta bucket
                    max_delta_bucket = max(max_delta_bucket, current_bucket_num);
                }
            }
        };
        
        // update data structures to remove an edge out of a node
        function<void(const handle_t&)> remove_outward_edge = [&](const handle_t& prev) {
            auto iter = degree_info.find(prev);
            if (iter != degree_info.end()) {
                // this node is in a delta bucket
                
                // remove it from the current bucket
                auto& degrees = iter->second.first;
                int64_t current_bucket_num = assign_bucket(degrees.first, degrees.second);
                auto& bucket = delta_buckets[current_bucket_num];
                bucket.erase(iter->second.second);
                
                // update the degrees to remove the outward edge
                degrees.second--;
                if (degrees.second == 0) {
                    // this is now a sink
                    degree_info.erase(prev);
                    sinks.push_back(prev);
                }
                else {
                    // this moves down one bucket
                    current_bucket_num--;
                    auto& new_bucket = delta_buckets[current_bucket_num];
                    new_bucket.emplace_front(prev);
                    iter->second.second = new_bucket.begin();
                }
            }
        };
        
        while (next_left_idx <= next_right_idx) {
            
#ifdef debug_eades
            cerr << "status of layout:" << endl;
            for (size_t i = 0; i < layout.size(); i++) {
                if (i >= next_left_idx || i <= next_right_idx) {
                    cerr << "\t." << endl;
                }
                else {
                    cerr << "\t" << graph->get_id(layout[i]) << (graph->get_is_reverse(layout[i]) ? "-" : "+") << endl;
                }
            }
            cerr << "status of sources:" << endl;
            for (handle_t h : sources) {
                cerr << "\t" << graph->get_id(h) << (graph->get_is_reverse(h) ? "-" : "+") << endl;
            }
            cerr << "status of sinks:" << endl;
            for (handle_t h : sinks) {
                cerr << "\t" << graph->get_id(h) << (graph->get_is_reverse(h) ? "-" : "+") << endl;
            }
            cerr << "status of delta buckets:" << endl;
            for (int64_t i = -layout.size() + 3; i < int(layout.size()) - 3; i++) {
                cerr << "\t" << i << " ";
                for (handle_t h : delta_buckets[i + layout.size() - 3]) {
                    cerr << "\t" << graph->get_id(h) << (graph->get_is_reverse(h) ? "-" : "+") << endl;
                }
                cerr << endl;
            }
#endif
            
            if (!sources.empty()) {
                // add a source to the layout
                handle_t source = sources.back();

#ifdef debug_eades
                cerr << "adding next source node " << graph->get_id(source) << (graph->get_is_reverse(source) ? "-" : "+") << endl;
#endif

                sources.pop_back();
                layout[next_left_idx] = source;
                next_left_idx++;
                
                // remove it from the graph
                graph->follow_edges(source, false, remove_inward_edge);
            }
            else if (!sinks.empty()) {
                // add a sink to the layout
                handle_t sink = sinks.back();
                
#ifdef debug_eades
                cerr << "adding next sink node " << graph->get_id(sink) << (graph->get_is_reverse(sink) ? "-" : "+") << endl;
#endif
                
                sinks.pop_back();
                layout[next_right_idx] = sink;
                next_right_idx--;
                
                // remove it from the graph
                graph->follow_edges(sink, true, remove_outward_edge);
            }
            else {
                // remove a node in the highest delta bucket from the graph
                auto& bucket = delta_buckets[max_delta_bucket];
                handle_t next = bucket.back();
                
#ifdef debug_eades
                cerr << "adding node " << graph->get_id(next) << (graph->get_is_reverse(next) ? "-" : "+") << " from delta bucket " << max_delta_bucket << endl;
#endif
                
                bucket.pop_back();
                degree_info.erase(next);
                
                // add it to the layout
                layout[next_left_idx] = next;
                next_left_idx++;
                
                graph->follow_edges(next, false, remove_inward_edge);
                graph->follow_edges(next, true, remove_outward_edge);
            }
            
            // move the max bucket lower if it has been emptied
            while (max_delta_bucket >= 0) {
                if (!delta_buckets[max_delta_bucket].empty()) {
                    break;
                }
                max_delta_bucket--;
            }
        }
        
        return layout;
    }
}
}
