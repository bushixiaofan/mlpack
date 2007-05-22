#ifndef NBR_UTILS_H
#define NBR_UTILS_H

#include "kdtree.h"

namespace nbr_utils {
  success_t Load(const char *fname, TempCacheArray<Vector> *cache_out,
      index_t vectors_per_block);

  template<typename Node, typename Param>
  success_t LoadKdTree(struct datanode *module,
      Param* param,
      TempCacheArray<Vector> *points_out,
      TempCacheArray<Node> *nodes_out) {
    index_t vectors_per_block = fx_param_int(
        module, "vectors_per_block", 256);
    success_t success;
    
    fx_timer_start(module, "read");
    success = nbr_utils::Load(fx_param_str_req(module, ""), points_out,
        vectors_per_block);
    fx_timer_stop(module, "read");
    
    if (success != SUCCESS_PASS) {
      // WALDO: Do something better?
      abort();
    }
    
    const Vector* first_point = points_out->StartRead(0);
    param->AnalyzePoint(*first_point);
    Node *example_node = new Node();
    example_node->Init(first_point->length(), *param);
    nodes_out->Init(*example_node, 0, 256);
    delete example_node;
    points_out->StopRead(0);
    
    fx_timer_start(module, "tree");
    KdTreeMidpointBuilder<Node, Param> builder;
    builder.InitBuild(module, param, points_out, nodes_out);
    fx_timer_stop(module, "tree");
    
    return SUCCESS_PASS;
  }
  
  template<typename GNP, typename Solver>
  void SerialDualTreeMain(datanode *module, const char *gnp_name) {
    typename GNP::Param param;
    
    param.Init(fx_submodule(module, gnp_name, gnp_name));

    TempCacheArray<typename GNP::Point> q_points;
    TempCacheArray<typename GNP::QNode> q_nodes;
    TempCacheArray<typename GNP::Point> r_points;
    TempCacheArray<typename GNP::RNode> r_nodes;
    TempCacheArray<typename GNP::QResult> q_results;

    nbr_utils::LoadKdTree(fx_submodule(module, "q", "q"),
        &param, &q_points, &q_nodes);
    nbr_utils::LoadKdTree(fx_submodule(module, "r", "r"),
        &param, &r_points, &r_nodes);

    typename GNP::QResult default_result;
    default_result.Init(param);
    q_results.Init(default_result, q_points.end_index(),
        q_points.n_block_elems());

    Solver solver;
    solver.Init(fx_submodule(module, "solver", "solver"), param,
        &q_points, &q_nodes,
        &r_points, &r_nodes, &q_results);
    solver.Begin();
  }
  /*
  template<typename GNP, typename Solver>
  void MpiDualTreeMain(datanode *module, const char *gnp_name) {
    RemoteObjectServer server;
    
    server.Init();
    
    int q_points_channel = server.NewTag();
    int q_point_infos_channel = server.NewTag();
    int q_nodes_channel = server.NewTag();
    int r_points_channel = server.NewTag();
    int r_point_infos_channel = server.NewTag();
    int r_nodes_channel = server.NewTag();
    int q_results_channel = server.NewTag();
    
    remove point-info as a separate array
    
    if (server) {
      TempCacheArray<typename GNP::Point> q_points;
      TempCacheArray<typename GNP::QNode> q_nodes;
      TempCacheArray<typename GNP::Point> r_points;
      TempCacheArray<typename GNP::RNode> r_nodes;
      TempCacheArray<typename GNP::QResult> q_results;
      
      export all of these arrays to the network
      
      typename GNP::Param param;

      param.Init(fx_submodule(module, gnp_name, gnp_name));

      nbr_utils::LoadKdTree(fx_submodule(module, "q", "q"),
          &param, &q_point_infos, &q_points, &q_nodes);
      nbr_utils::LoadKdTree(fx_submodule(module, "r", "r"),
          &param, &r_point_infos, &r_points, &r_nodes);

      typename GNP::QResult default_result;
      default_result.Init(param);
      q_results.Init(default_result, q_points.end_index(),
          q_points.n_block_elems());

      server.Loop();
    } else if (worker) {
      MPI_Barrier();
      
      initialize them all with default elements
      
      NetCacheArray<typename GNP::Point> q_points;
      q_points.Init(q_points_channel, BlockDevice::READ);
      NetCacheArray<typename GNP::QNode> q_nodes;
      q_nodes.Init(q_nodes_channel, BlockDevice::READ);
      NetCacheArray<typename GNP::Point> r_points;
      r_points.Init(r_points_channel, BlockDevice::READ);
      NetCacheArray<typename GNP::RNode> r_nodes;
      r_nodes.Init(r_nodes_channel, BlockDevice::READ);
      NetCacheArray<typename GNP::QResult> q_results;
      q_results.Init(q_results_channel, BlockDevice::CREATE);
      
      while (work) {
        Solver solver;
        solver.Init(fx_submodule(module, "solver", "solver"), param,
            &q_points, &q_point_infos, &q_nodes,
            &r_points, &r_point_infos, &r_nodes, &q_results);
        solver.Begin();
      }
    }
  }
  */
};

#endif

