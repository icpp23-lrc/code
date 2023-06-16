#include "coordinator.h"
#include "azure_lrc.h"
#include "tinyxml2.h"
#include <cfloat>
#include <chrono>
#include <climits>
#include <fstream>
#include <random>
#include <thread>
using namespace std;
using namespace chrono;

namespace OppoProject {

template <typename T> inline T ceil(T const &A, T const &B) {
  return T((A + B - 1) / B);
};

grpc::Status CoordinatorImpl::setParameter(
    ::grpc::ServerContext *context,
    const coordinator_proto::Parameter *parameter,
    coordinator_proto::RepIfSetParaSucess *setParameterReply) {
  ECSchema system_metadata(
      parameter->partial_decoding(),
      (OppoProject::EncodeType)parameter->encodetype(),
      (OppoProject::PlacementType)parameter->placementtype(),
      parameter->k_datablock(), parameter->real_l_localgroup(),
      parameter->g_m_globalparityblock(), parameter->b_datapergoup(),
      parameter->small_file_upper(), parameter->blob_size_upper());
  m_encode_parameter = system_metadata;
  alpha = ((double)(parameter->alpha())) / 100;
  setParameterReply->set_ifsetparameter(true);
  std::cout << "setParameter success" << std::endl;
  return grpc::Status::OK;
}

grpc::Status CoordinatorImpl::sayHelloToCoordinator(
    ::grpc::ServerContext *context,
    const coordinator_proto::RequestToCoordinator *helloRequestToCoordinator,
    coordinator_proto::ReplyFromCoordinator *helloReplyFromCoordinator) {
  std::string prefix("Hello ");
  helloReplyFromCoordinator->set_message(prefix +
                                         helloRequestToCoordinator->name());
  std::cout << prefix + helloRequestToCoordinator->name() << std::endl;
  return grpc::Status::OK;
}

void CoordinatorImpl::compute_cost_for_az(AZitem &az, double &storage_cost,
                                          double &network_cost) {
  double all_storage = 0, all_bandwidth = 0;
  double all_storage_cost = 0, all_network_cost = 0;
  for (int i = 0; i < int(az.nodes.size()); i++) {
    int node_id = az.nodes[i];
    Nodeitem &node_info = m_Node_info[node_id];
    all_storage += node_info.storage;
    all_bandwidth += node_info.bandwidth;
    all_storage_cost += node_info.storage_cost;
    all_network_cost += node_info.network_cost;
  }
  storage_cost = all_storage_cost / all_storage;
  network_cost = all_network_cost / all_bandwidth;
}

grpc::Status
CoordinatorImpl::checkBias(::grpc::ServerContext *context,
                           const ::coordinator_proto::myVoid *request,
                           ::coordinator_proto::checkBiasResult *response) {
  response->set_cross_repair_traffic(cross_repair_traffic);
  response->set_degraded_time(degraded_time);
  response->set_all_time(all_time);
  {
    double max_storage_cost = DBL_MIN, avg_storage_cost = 0;
    double max_network_cost = DBL_MIN, avg_network_cost = 0;
    double storage_bias = 0;
    double network_bias = 0;
    for (auto &p : m_Node_info) {
      double storage_cost = p.second.storage_cost / p.second.storage;
      double network_cost = p.second.network_cost / p.second.bandwidth;
      max_storage_cost = std::max(max_storage_cost, storage_cost);
      max_network_cost = std::max(max_network_cost, network_cost);
      avg_storage_cost += storage_cost;
      avg_network_cost += network_cost;
    }
    avg_storage_cost /= (double)m_Node_info.size();
    avg_network_cost /= (double)m_Node_info.size();
    storage_bias = (max_storage_cost - avg_storage_cost) / avg_storage_cost;
    network_bias = (max_network_cost - avg_network_cost) / avg_network_cost;
    response->set_node_storage_bias(storage_bias);
    response->set_node_network_bias(network_bias);
    std::cout << "Node Level!!!!" << std::endl;
    std::cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
              << std::endl;
    std::cout << "storage_bias: " << storage_bias
              << ", network_bias: " << network_bias << std::endl;
    std::cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
              << std::endl;
  }

  {
    double max_storage_cost = DBL_MIN, avg_storage_cost = 0;
    double max_network_cost = DBL_MIN, avg_network_cost = 0;
    double storage_bias = 0;
    double network_bias = 0;
    for (auto &p : m_AZ_info) {
      double storage_cost = 0, network_cost = 0;
      ;
      compute_cost_for_az(p.second, storage_cost, network_cost);
      max_storage_cost = std::max(max_storage_cost, storage_cost);
      max_network_cost = std::max(max_network_cost, network_cost);
      avg_storage_cost += storage_cost;
      avg_network_cost += network_cost;
    }
    avg_storage_cost /= (double)m_AZ_info.size();
    avg_network_cost /= (double)m_AZ_info.size();
    storage_bias = (max_storage_cost - avg_storage_cost) / avg_storage_cost;
    network_bias = (max_network_cost - avg_network_cost) / avg_network_cost;
    response->set_az_storage_bias(storage_bias);
    response->set_az_network_bias(network_bias);
    std::cout << "AZ Level!!!!" << std::endl;
    std::cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
              << std::endl;
    std::cout << "storage_bias: " << storage_bias
              << ", network_bias: " << network_bias << std::endl;
    std::cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
              << std::endl;
  }

  for (auto &p : m_Node_info) {
    std::cout << "node_id: " << p.first
              << ", node network: " << p.second.network_cost
              << ", node storage: " << p.second.storage_cost << std::endl;
  }
  return grpc::Status::OK;
}

void CoordinatorImpl::compute_avg(double &node_avg_storage_cost,
                                  double &node_avg_network_cost,
                                  double &az_avg_storage_cost,
                                  double &az_avg_network_cost) {
  for (auto &p : m_Node_info) {
    double storage_cost = p.second.storage_cost / p.second.storage;
    double network_cost = p.second.network_cost / p.second.bandwidth;
    node_avg_storage_cost += storage_cost;
    node_avg_network_cost += network_cost;
  }
  node_avg_storage_cost /= (double)m_Node_info.size();
  node_avg_network_cost /= (double)m_Node_info.size();

  for (auto &p : m_AZ_info) {
    double storage_cost = 0, network_cost = 0;
    ;
    compute_cost_for_az(p.second, storage_cost, network_cost);
    az_avg_storage_cost += storage_cost;
    az_avg_network_cost += network_cost;
  }
  az_avg_storage_cost /= (double)m_AZ_info.size();
  az_avg_network_cost /= (double)m_AZ_info.size();
}

grpc::Status CoordinatorImpl::uploadOriginKeyValue(
    ::grpc::ServerContext *context,
    const coordinator_proto::RequestProxyIPPort *keyValueSize,
    coordinator_proto::ReplyProxyIPPort *proxyIPPort) {

  std::string key = keyValueSize->key();
  m_mutex.lock();
  m_object_table_big_small_commit.erase(key);
  m_mutex.unlock();
  int valuesizebytes = keyValueSize->valuesizebytes();

  ObjectItemBigSmall new_object;

  int k = m_encode_parameter.k_datablock;
  int m = m_encode_parameter.g_m_globalparityblock;
  int real_l = m_encode_parameter.real_l_localgroup;
  int b = m_encode_parameter.b_datapergoup;
  new_object.object_size = valuesizebytes;

  if (valuesizebytes > m_encode_parameter.small_file_upper) {
    new_object.big_object = true;
    if (valuesizebytes > m_encode_parameter.blob_size_upper) {
      proxy_proto::ObjectAndPlacement object_placement;
      object_placement.set_encode_type((int)m_encode_parameter.encodetype);
      object_placement.set_bigobject(true);
      object_placement.set_key(key);
      object_placement.set_valuesizebyte(valuesizebytes);
      object_placement.set_k(k);
      object_placement.set_m(m);
      object_placement.set_real_l(real_l);
      object_placement.set_b(b);
      int shard_size = ceil(m_encode_parameter.blob_size_upper, k);
      shard_size = 16 * ceil(shard_size, 16);
      object_placement.set_shard_size(shard_size);

      int num_of_stripes = valuesizebytes / (k * shard_size);
      for (int i = 0; i < num_of_stripes; i++) {
        valuesizebytes -= k * shard_size;
        StripeItem temp;
        temp.Stripe_id = m_next_stripe_id++;
        m_Stripe_info[temp.Stripe_id] = temp;
        StripeItem &stripe = m_Stripe_info[temp.Stripe_id];
        stripe.shard_size = shard_size;
        stripe.k = k;
        stripe.real_l = real_l;
        stripe.g_m = m;
        stripe.b = b;
        stripe.placementtype = m_encode_parameter.placementtype;
        stripe.encodetype = m_encode_parameter.encodetype;
        // for (int i = 0; i < k + m; i++) {
        //   // 其实应该根据placement_plan来添加node_id
        //   stripe.nodes.push_back(i);
        // }
        generate_placement(m_Stripe_info[stripe.Stripe_id].nodes,
                           stripe.Stripe_id);
        new_object.stripes.push_back(stripe.Stripe_id);
        for (int i = 0; i < int(m_Stripe_info[stripe.Stripe_id].nodes.size());
             i++) {
          int node_id = m_Stripe_info[stripe.Stripe_id].nodes[i];
          m_Node_info[node_id].network_cost += 1;
          m_Node_info[node_id].storage_cost += 1;
        }

        object_placement.add_stripe_ids(stripe.Stripe_id);
        for (int i = 0; i < int(stripe.nodes.size()); i++) {
          Nodeitem &node = m_Node_info[stripe.nodes[i]];
          object_placement.add_datanodeip(node.Node_ip.c_str());
          object_placement.add_datanodeport(node.Node_port);
        }
      }
      if (valuesizebytes > 0) {
        int shard_size = ceil(valuesizebytes, k);
        shard_size = 16 * ceil(shard_size, 16);
        StripeItem temp;
        temp.Stripe_id = m_next_stripe_id++;
        m_Stripe_info[temp.Stripe_id] = temp;
        StripeItem &stripe = m_Stripe_info[temp.Stripe_id];
        stripe.shard_size = shard_size;
        stripe.k = k;
        stripe.real_l = real_l;
        stripe.g_m = m;
        stripe.b = b;
        stripe.placementtype = m_encode_parameter.placementtype;
        stripe.encodetype = m_encode_parameter.encodetype;
        // for (int i = 0; i < k + m; i++) {
        //   // 其实应该根据placement_plan来添加node_id
        //   stripe.nodes.push_back(i);
        // }
        generate_placement(m_Stripe_info[stripe.Stripe_id].nodes,
                           stripe.Stripe_id);
        new_object.stripes.push_back(stripe.Stripe_id);
        for (int i = 0; i < int(m_Stripe_info[stripe.Stripe_id].nodes.size());
             i++) {
          int node_id = m_Stripe_info[stripe.Stripe_id].nodes[i];
          m_Node_info[node_id].network_cost += 1;
          m_Node_info[node_id].storage_cost += 1;
        }

        object_placement.add_stripe_ids(stripe.Stripe_id);
        object_placement.set_tail_shard_size(shard_size);
        for (int i = 0; i < int(stripe.nodes.size()); i++) {
          Nodeitem &node = m_Node_info[stripe.nodes[i]];
          object_placement.add_datanodeip(node.Node_ip.c_str());
          object_placement.add_datanodeport(node.Node_port);
        }
      } else {
        object_placement.set_tail_shard_size(-1);
      }
      grpc::ClientContext handle_ctx;
      proxy_proto::SetReply set_reply;
      grpc::Status status;
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dis(0, m_AZ_info.size() - 1);
      int az_id = dis(gen);
      std::string selected_proxy_ip = m_AZ_info[az_id].proxy_ip;
      int selected_proxy_port = m_AZ_info[az_id].proxy_port;
      std::string choose_proxy =
          selected_proxy_ip + ":" + std::to_string(selected_proxy_port);
      status = m_proxy_ptrs[choose_proxy]->EncodeAndSetObject(
          &handle_ctx, object_placement, &set_reply);
      proxyIPPort->set_proxyip(selected_proxy_ip);
      proxyIPPort->set_proxyport(selected_proxy_port + 1);
    } else {
      int shard_size = ceil(valuesizebytes, k);
      shard_size = 16 * ceil(shard_size, 16);
      StripeItem temp;
      temp.Stripe_id = m_next_stripe_id++;
      m_Stripe_info[temp.Stripe_id] = temp;
      StripeItem &stripe = m_Stripe_info[temp.Stripe_id];
      stripe.shard_size = shard_size;
      stripe.k = k;
      stripe.real_l = real_l;
      stripe.g_m = m;
      stripe.b = b;
      stripe.placementtype = m_encode_parameter.placementtype;
      stripe.encodetype = m_encode_parameter.encodetype;
      generate_placement(m_Stripe_info[stripe.Stripe_id].nodes,
                         stripe.Stripe_id);
      new_object.stripes.push_back(stripe.Stripe_id);
      for (int i = 0; i < int(m_Stripe_info[stripe.Stripe_id].nodes.size());
           i++) {
        int node_id = m_Stripe_info[stripe.Stripe_id].nodes[i];
        m_Node_info[node_id].network_cost += 1;
        m_Node_info[node_id].storage_cost += 1;
      }

      grpc::ClientContext handle_ctx;
      proxy_proto::SetReply set_reply;
      grpc::Status status;
      proxy_proto::ObjectAndPlacement object_placement;

      object_placement.set_encode_type((int)stripe.encodetype);
      object_placement.add_stripe_ids(stripe.Stripe_id);
      object_placement.set_bigobject(true);
      object_placement.set_key(key);
      object_placement.set_valuesizebyte(valuesizebytes);
      object_placement.set_k(k);
      object_placement.set_m(m);
      object_placement.set_real_l(real_l);
      object_placement.set_b(b);
      object_placement.set_shard_size(shard_size);
      object_placement.set_tail_shard_size(-1);
      for (int i = 0; i < int(stripe.nodes.size()); i++) {
        Nodeitem &node = m_Node_info[stripe.nodes[i]];
        object_placement.add_datanodeip(node.Node_ip.c_str());
        object_placement.add_datanodeport(node.Node_port);
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dis(0, m_AZ_info.size() - 1);
      int az_id = dis(gen);
      std::string selected_proxy_ip = m_AZ_info[az_id].proxy_ip;
      int selected_proxy_port = m_AZ_info[az_id].proxy_port;
      std::string choose_proxy =
          selected_proxy_ip + ":" + std::to_string(selected_proxy_port);
      status = m_proxy_ptrs[choose_proxy]->EncodeAndSetObject(
          &handle_ctx, object_placement, &set_reply);
      proxyIPPort->set_proxyip(selected_proxy_ip);
      proxyIPPort->set_proxyport(selected_proxy_port + 1);

      if (status.ok()) {
      } else {
        std::cout << "datanodes can not serve client download request!"
                  << std::endl;
        return grpc::Status::CANCELLED;
      }
    }
  } else {
    // future work about small file
  }
  std::unique_lock<std::mutex> lck(m_mutex);
  m_object_table_big_small_updating[key] = new_object;

  return grpc::Status::OK;
}

grpc::Status
CoordinatorImpl::getValue(::grpc::ServerContext *context,
                          const coordinator_proto::KeyAndClientIP *keyClient,
                          coordinator_proto::RepIfGetSucess *getReplyClient) {
  try {
    std::string key = keyClient->key();
    std::string client_ip = keyClient->clientip();
    int client_port = keyClient->clientport();
    m_mutex.lock();
    ObjectItemBigSmall object_infro = m_object_table_big_small_commit.at(key);
    m_mutex.unlock();
    int k = m_Stripe_info[object_infro.stripes[0]].k;
    int m = m_Stripe_info[object_infro.stripes[0]].g_m;
    int real_l = m_Stripe_info[object_infro.stripes[0]].real_l;
    int b = m_Stripe_info[object_infro.stripes[0]].b;

    grpc::ClientContext decode_and_get;
    proxy_proto::ObjectAndPlacement object_placement;
    grpc::Status status;
    proxy_proto::GetReply get_reply;
    getReplyClient->set_valuesizebytes(object_infro.object_size);
    if (object_infro.big_object) {
      object_placement.set_bigobject(true);
      object_placement.set_key(key);
      object_placement.set_valuesizebyte(object_infro.object_size);
      object_placement.set_k(k);
      object_placement.set_m(m);
      object_placement.set_real_l(real_l);
      object_placement.set_b(b);
      object_placement.set_tail_shard_size(-1);
      if (object_infro.object_size > m_encode_parameter.blob_size_upper) {
        int shard_size = ceil(m_encode_parameter.blob_size_upper, k);
        shard_size = 16 * ceil(shard_size, 16);
        object_placement.set_shard_size(shard_size);
        if (object_infro.object_size % (k * shard_size) != 0) {
          int tail_stripe_size = object_infro.object_size % (k * shard_size);
          int tail_shard_size = ceil(tail_stripe_size, k);
          tail_shard_size = 16 * ceil(tail_shard_size, 16);
          object_placement.set_tail_shard_size(tail_shard_size);
        }
      } else {
        int shard_size = ceil(object_infro.object_size, k);
        shard_size = 16 * ceil(shard_size, 16);
        object_placement.set_shard_size(shard_size);
      }

      for (int i = 0; i < int(object_infro.stripes.size()); i++) {
        StripeItem &stripe = m_Stripe_info[object_infro.stripes[i]];
        for (int j = 0; j <= k - 1; j++) {
          int node_id = stripe.nodes[j];
          m_Node_info[node_id].network_cost += 1;
        }
        object_placement.set_encode_type((int)stripe.encodetype);
        object_placement.add_stripe_ids(stripe.Stripe_id);
        for (int j = 0; j < int(stripe.nodes.size()); j++) {
          Nodeitem &node = m_Node_info[stripe.nodes[j]];
          object_placement.add_datanodeip(node.Node_ip.c_str());
          object_placement.add_datanodeport(node.Node_port);
        }
      }

      object_placement.set_clientip(client_ip);
      object_placement.set_clientport(client_port);

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dis(0, m_AZ_info.size() - 1);
      std::string choose_proxy = m_AZ_info[dis(gen)].proxy_ip + ":" +
                                 std::to_string(m_AZ_info[dis(gen)].proxy_port);
      status = m_proxy_ptrs[choose_proxy]->decodeAndGetObject(
          &decode_and_get, object_placement, &get_reply);
    } else {
      // future work about small file
    }
  } catch (std::exception &e) {
    std::cout << "getValue exception" << std::endl;
    std::cout << e.what() << std::endl;
  }
  return grpc::Status::OK;
}
grpc::Status CoordinatorImpl::checkalive(
    grpc::ServerContext *context,
    const coordinator_proto::RequestToCoordinator *helloRequestToCoordinator,
    coordinator_proto::ReplyFromCoordinator *helloReplyFromCoordinator) {

  std::cout << "checkalive" << helloRequestToCoordinator->name() << std::endl;
  return grpc::Status::OK;
}
grpc::Status CoordinatorImpl::reportCommitAbort(
    grpc::ServerContext *context,
    const coordinator_proto::CommitAbortKey *commit_abortkey,
    coordinator_proto::ReplyFromCoordinator *helloReplyFromCoordinator) {
  std::string key = commit_abortkey->key();
  std::unique_lock<std::mutex> lck(m_mutex);
  try {
    if (commit_abortkey->ifcommitmetadata()) {
      m_object_table_big_small_commit[key] =
          m_object_table_big_small_updating[key];
      cv.notify_all();

      m_object_table_big_small_updating.erase(key);
    } else {
      m_object_table_big_small_updating.erase(key);
    }
  } catch (std::exception &e) {
    std::cout << "reportCommitAbort exception" << std::endl;
    std::cout << e.what() << std::endl;
  }
  return grpc::Status::OK;
}

grpc::Status
CoordinatorImpl::checkCommitAbort(grpc::ServerContext *context,
                                  const coordinator_proto::AskIfSetSucess *key,
                                  coordinator_proto::RepIfSetSucess *reply) {
  std::unique_lock<std::mutex> lck(m_mutex);
  while (m_object_table_big_small_commit.find(key->key()) ==
         m_object_table_big_small_commit.end()) {
    cv.wait(lck);
  }
  reply->set_ifcommit(true);
  return grpc::Status::OK;
}

grpc::Status CoordinatorImpl::requestRepair(
    ::grpc::ServerContext *context,
    const coordinator_proto::FailNodes *failed_node_list,
    coordinator_proto::RepIfRepairSucess *reply) {
  std::vector<int> failed_node_ids;
  for (int i = 0; i < failed_node_list->node_list_size(); i++) {
    int node_id = failed_node_list->node_list(i);
    failed_node_ids.push_back(node_id);
  }

  std::unordered_set<int> failed_stripe_ids;
  for (auto &node_id : failed_node_ids) {
    for (auto &stripe_id : m_Node_info[node_id].stripes) {
      failed_stripe_ids.insert(stripe_id);
    }
  }
  std::cout << failed_stripe_ids.size() << std::endl;
  for (auto stripe_id : failed_stripe_ids) {
    bool test_bias = false;
    if (test_bias) {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dis(1,
                                                      failed_stripe_ids.size());
      int temp = dis(gen);
      int count = 0;
      for (auto &p : failed_stripe_ids) {
        stripe_id = p;
        count++;
        if (count >= temp) {
          break;
        }
      }
    }
    StripeItem &stripe_info = m_Stripe_info[stripe_id];
    std::vector<int> failed_shard_idxs;
    for (size_t i = 0; i < m_Stripe_info[stripe_id].nodes.size(); i++) {
      auto lookup = std::find(failed_node_ids.begin(), failed_node_ids.end(),
                              m_Stripe_info[stripe_id].nodes[i]);
      if (lookup != failed_node_ids.end()) {
        failed_shard_idxs.push_back(i);
      }
    }

    if (stripe_info.encodetype == Azure_LRC) {
      if (failed_shard_idxs.size() > 0) {
        std::cout << "stripe id: " << stripe_id
                  << ", repair index: " << failed_shard_idxs[0] << std::endl;
        do_repair(stripe_id, {failed_shard_idxs[0]});
      }
      if (test_bias) {
        break;
      }
      continue;
    }

  }
  reply->set_ifrepair(true);
  return grpc::Status::OK;
}

bool num_survive_nodes(std::pair<int, std::vector<std::pair<int, int>>> &a,
                       std::pair<int, std::vector<std::pair<int, int>>> &b) {
  return a.second.size() > b.second.size();
}

bool cmp_num_live_shards(std::pair<int, std::vector<int>> &a,
                         std::pair<int, std::vector<int>> &b) {
  return a.second > b.second;
}

bool cmp_num_shards_MDS(std::pair<int, std::vector<int>> &a,
                        std::pair<int, std::vector<int>> &b) {
  return a.second.size() > b.second.size();
}

void CoordinatorImpl::generate_repair_plan(
    int stripe_id, bool one_shard, std::vector<int> &failed_shard_idxs,
    std::vector<std::vector<std::pair<std::pair<std::string, int>, int>>>
        &shards_to_read,
    std::vector<int> &repair_span_az,
    std::vector<std::pair<int, int>> &new_locations_with_shard_idx,
    std::unordered_map<int, bool> &merge) {
  StripeItem &stripe_info = m_Stripe_info[stripe_id];
  int k = stripe_info.k;
  int real_l = stripe_info.real_l;
  int g = stripe_info.g_m;
  int b = stripe_info.b;
  if (one_shard) {
    int failed_shard_idx = failed_shard_idxs[0];
    Nodeitem &failed_node_info =
        m_Node_info[stripe_info.nodes[failed_shard_idx]];
    int main_az_id = failed_node_info.AZ_id;
    repair_span_az.push_back(main_az_id);
    new_locations_with_shard_idx.push_back(
        {failed_node_info.Node_id, failed_shard_idx});
    if (stripe_info.encodetype == Azure_LRC) {
      if (failed_shard_idx >= k && failed_shard_idx <= (k + g - 1)) {
        // 单全局校验块修复
        std::unordered_map<int, std::vector<int>> live_shards_MDS;
        std::unordered_map<int, std::vector<int>> live_shards_MDS_need;
        for (int i = 0; i <= (k + g - 1); i++) {
          Nodeitem &node_info = m_Node_info[stripe_info.nodes[i]];
          if (i != failed_shard_idx) {
            live_shards_MDS[node_info.AZ_id].push_back(i);
          }
        }
        int num_shards_need = k;
        for (auto &p : live_shards_MDS[main_az_id]) {
          if (num_shards_need <= 0) {
            break;
          }
          live_shards_MDS_need[main_az_id].push_back(p);
          num_shards_need--;
        }
        std::vector<std::pair<int, std::vector<int>>> sorted_live_shards_MDS;
        for (auto &p : live_shards_MDS) {
          if (p.first != main_az_id) {
            sorted_live_shards_MDS.push_back({p.first, p.second});
          }
        }
        std::sort(sorted_live_shards_MDS.begin(), sorted_live_shards_MDS.end(),
                  cmp_num_shards_MDS);
        for (auto &p : sorted_live_shards_MDS) {
          for (auto &q : p.second) {
            if (num_shards_need <= 0) {
              break;
            }
            live_shards_MDS_need[p.first].push_back(q);
            num_shards_need--;
          }
        }
        std::vector<std::pair<std::pair<std::string, int>, int>> temp;
        for (auto &p : live_shards_MDS_need[main_az_id]) {
          Nodeitem &node_info = m_Node_info[stripe_info.nodes[p]];
          node_info.network_cost += 1;
          temp.push_back({{node_info.Node_ip, node_info.Node_port}, p});
        }
        shards_to_read.push_back(temp);
        for (auto &p : live_shards_MDS_need) {
          if (p.first != main_az_id) {
            repair_span_az.push_back(p.first);
            std::vector<std::pair<std::pair<std::string, int>, int>> temp;
            for (auto &q : p.second) {
              Nodeitem &node_info = m_Node_info[stripe_info.nodes[q]];
              node_info.network_cost += 1;
              temp.push_back({{node_info.Node_ip, node_info.Node_port}, q});
            }
            shards_to_read.push_back(temp);
          }
        }
      } else {
        // 单数据块修复
        int group_idx;
        if (failed_shard_idx >= 0 && failed_shard_idx <= (k - 1)) {
          group_idx = failed_shard_idx / b;
        } else {
          group_idx = failed_shard_idx - (k + g);
        }
        std::vector<std::pair<int, int>> live_shards_in_group;
        for (int i = 0; i < b; i++) {
          int idx = group_idx * b + i;
          if (idx == failed_shard_idx) {
            continue;
          }
          if (idx >= k) {
            break;
          }
          live_shards_in_group.push_back({stripe_info.nodes[idx], idx});
        }
        if (k + g + group_idx != failed_shard_idx) {
          live_shards_in_group.push_back(
              {stripe_info.nodes[k + g + group_idx], k + g + group_idx});
        }
        std::unordered_set<int> live_shards_group_span_az;
        for (auto &shard : live_shards_in_group) {
          live_shards_group_span_az.insert(m_Node_info[shard.first].AZ_id);
        }
        for (auto &az_id : live_shards_group_span_az) {
          if (az_id != main_az_id) {
            repair_span_az.push_back(az_id);
          }
        }
        for (auto &az_id : repair_span_az) {
          std::vector<std::pair<std::pair<std::string, int>, int>> temp4;
          for (auto &live_shard : live_shards_in_group) {
            Nodeitem &node_info = m_Node_info[live_shard.first];
            if (node_info.AZ_id == az_id) {
              node_info.network_cost += 1;
              temp4.push_back({{node_info.Node_ip, node_info.Node_port},
                               live_shard.second});
            }
          }
          shards_to_read.push_back(temp4);
        }
      }
    }
  } else {
    // future work about multi block repair
  }
  return;
}

void CoordinatorImpl::do_repair(int stripe_id,
                                std::vector<int> failed_shard_idxs) {
  StripeItem &stripe_info = m_Stripe_info[stripe_id];
  if (failed_shard_idxs.size() == 1) {
    std::cout << "marker 1" << std::endl;
    std::vector<std::vector<std::pair<std::pair<std::string, int>, int>>>
        shards_to_read;
    std::vector<int> repair_span_az;
    std::vector<std::pair<int, int>> new_locations_with_shard_idx;
    std::unordered_map<int, bool> merge;
    generate_repair_plan(stripe_id, true, failed_shard_idxs, shards_to_read,
                         repair_span_az, new_locations_with_shard_idx, merge);
    cross_repair_traffic += (repair_span_az.size() - 1);
    int main_az_id = repair_span_az[0];
    bool multi_az = (repair_span_az.size() > 1);
    std::cout << "repair_span_az: ";
    for (auto &p : repair_span_az) {
      std::cout << p << " ";
    }
    std::cout << std::endl;
    for (int i = 0; i < shards_to_read.size(); i++) {
      int az_id = repair_span_az[i];
      std::cout << "az_id: " << az_id << std::endl;
      for (int j = 0; j < shards_to_read[i].size(); j++) {
        std::cout << shards_to_read[i][j].second << " ";
      }
      std::cout << std::endl;
    }

    std::vector<std::thread> repairs;
    for (int i = 0; i < int(repair_span_az.size()); i++) {
      int az_id = repair_span_az[i];
      if (i == 0) {
        // 第1个az是main az
        std::cout << "main_az_id: " << az_id << std::endl;
        repairs.push_back(std::thread([&, i, az_id]() {
          grpc::ClientContext context;
          proxy_proto::mainRepairPlan request;
          proxy_proto::mainRepairReply reply;
          request.set_encode_type(int(stripe_info.encodetype));
          request.set_one_shard_fail(true);
          request.set_multi_az(multi_az);
          request.set_k(stripe_info.k);
          request.set_real_l(stripe_info.real_l);
          request.set_g(stripe_info.g_m);
          request.set_b(stripe_info.b);
          request.set_if_partial_decoding(m_encode_parameter.partial_decoding);
          for (int j = 0; j < int(shards_to_read[0].size()); j++) {
            request.add_inner_az_help_shards_ip(
                shards_to_read[0][j].first.first);
            request.add_inner_az_help_shards_port(
                shards_to_read[0][j].first.second);
            request.add_inner_az_help_shards_idx(shards_to_read[0][j].second);
          }
          for (int j = 0; j < shards_to_read.size(); j++) {
            for (int t = 0; t < shards_to_read[j].size(); t++) {
              request.add_chosen_shards(shards_to_read[j][t].second);
            }
          }
          for (int j = 0; j < failed_shard_idxs.size(); j++) {
            request.add_all_failed_shards_idx(failed_shard_idxs[j]);
          }
          for (int j = 0; j < int(new_locations_with_shard_idx.size()); j++) {
            Nodeitem &node_info =
                m_Node_info[new_locations_with_shard_idx[j].first];
            request.add_new_location_ip(node_info.Node_ip);
            request.add_new_location_port(node_info.Node_port);
            request.add_new_location_shard_idx(
                new_locations_with_shard_idx[j].second);
          }
          request.set_self_az_id(az_id);
          for (auto &help_az_id : repair_span_az) {
            if (help_az_id != main_az_id) {
              request.add_help_azs_id(help_az_id);
            }
          }
          request.set_shard_size(stripe_info.shard_size);
          request.set_stripe_id(stripe_id);
          std::string main_ip_port =
              m_AZ_info[main_az_id].proxy_ip + ":" +
              std::to_string(m_AZ_info[main_az_id].proxy_port);
          m_proxy_ptrs[main_ip_port]->mainRepair(&context, request, &reply);
          all_time += reply.time_cost();
          if (failed_shard_idxs.size() == 1 &&
              failed_shard_idxs[0] < m_encode_parameter.k_datablock) {
            degraded_time += reply.time_cost();
          }
          std::cout << "main_az_done: " << az_id << std::endl;
        }));
      } else {
        std::cout << "help_az_id: " << az_id << std::endl;
        std::thread([&, i, az_id]() {
          grpc::ClientContext context;
          proxy_proto::helpRepairPlan request;
          proxy_proto::helpRepairReply reply;
          request.set_encode_type(int(stripe_info.encodetype));
          request.set_one_shard_fail(true);
          request.set_multi_az(multi_az);
          request.set_k(stripe_info.k);
          request.set_real_l(stripe_info.real_l);
          request.set_g(stripe_info.g_m);
          request.set_b(stripe_info.b);
          request.set_if_partial_decoding(m_encode_parameter.partial_decoding);
          for (int j = 0; j < int(shards_to_read[i].size()); j++) {
            request.add_inner_az_help_shards_ip(
                shards_to_read[i][j].first.first);
            request.add_inner_az_help_shards_port(
                shards_to_read[i][j].first.second);
            request.add_inner_az_help_shards_idx(shards_to_read[i][j].second);
          }
          for (int j = 0; j < shards_to_read.size(); j++) {
            for (int t = 0; t < shards_to_read[j].size(); t++) {
              request.add_chosen_shards(shards_to_read[j][t].second);
            }
          }
          for (int j = 0; j < failed_shard_idxs.size(); j++) {
            request.add_all_failed_shards_idx(failed_shard_idxs[j]);
          }
          request.set_shard_size(stripe_info.shard_size);
          request.set_main_proxy_ip(m_AZ_info[main_az_id].proxy_ip);
          request.set_main_proxy_port(m_AZ_info[main_az_id].proxy_port + 1);
          request.set_stripe_id(stripe_id);
          request.set_failed_shard_idx(failed_shard_idxs[0]);
          request.set_self_az_id(az_id);
          std::string help_ip_port =
              m_AZ_info[az_id].proxy_ip + ":" +
              std::to_string(m_AZ_info[az_id].proxy_port);
          std::cout << "m_AZ_info[az_id].proxy_ip " << m_AZ_info[az_id].proxy_ip
                    << ", m_AZ_info[az_id].proxy_port: "
                    << m_AZ_info[az_id].proxy_port << std::endl;
          m_proxy_ptrs[help_ip_port]->helpRepair(&context, request, &reply);
          std::cout << "help_az_done: " << az_id << std::endl;
        }).join();
      }
    }
    for (int i = 0; i < int(repairs.size()); i++) {
      std::cout << "marker: " << i << std::endl;
      repairs[i].join();
    }
    std::cout << "new_locations_with_shard_idx[0].second index: "
              << new_locations_with_shard_idx[0].second << std::endl;
    std::cout << "new_locations_with_shard_idx[0].first node_id: "
              << new_locations_with_shard_idx[0].first << std::endl;
    // m_Stripe_info[stripe_id].nodes[new_locations_with_shard_idx[0].second] =
    // new_locations_with_shard_idx[0].first;
  }
  std::cout << "repair everything done: " << failed_shard_idxs[0] << std::endl;
}

bool CoordinatorImpl::init_proxy(std::string proxy_information_path) {
  for (auto cur = m_AZ_info.begin(); cur != m_AZ_info.end(); cur++) {
    std::string proxy_ip_and_port =
        cur->second.proxy_ip + ":" + std::to_string(cur->second.proxy_port);
    auto _stub = proxy_proto::proxyService::NewStub(grpc::CreateChannel(
        proxy_ip_and_port, grpc::InsecureChannelCredentials()));
    proxy_proto::CheckaliveCMD Cmd;
    proxy_proto::RequestResult result;
    grpc::ClientContext clientContext;
    Cmd.set_name("wwwwwwwww");
    grpc::Status status;
    status = _stub->checkalive(&clientContext, Cmd, &result);
    if (status.ok()) {
      std::cout << "checkalive,ok" << std::endl;
    } else {
      std::cout << "checkalive,fail" << std::endl;
    }
    m_proxy_ptrs.insert(std::make_pair(proxy_ip_and_port, std::move(_stub)));
  }
  return true;
}
bool CoordinatorImpl::init_AZinformation(std::string Azinformation_path) {
  std::vector<double> storages = {
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 64,
      64, 64, 64, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 64, 64, 64, 64, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 64, 64, 64, 64, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 64, 64, 64, 64, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 64, 64, 64, 64, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 64, 64, 64,
      64, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      64, 64, 64, 64, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};
  std::vector<double> bandwidth = {
      1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  10,
      10, 10, 10, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
      1,  1,  10, 10, 10, 10, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
      1,  1,  1,  1,  1,  10, 10, 10, 10, 1,  1,  1,  1,  1,  1,  1,  1,
      1,  1,  1,  1,  1,  1,  1,  1,  10, 10, 10, 10, 1,  1,  1,  1,  1,
      1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  10, 10, 10, 10, 1,  1,
      1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  10, 10, 10,
      10, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
      10, 10, 10, 10, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
      1,  1,  1,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
  std::cout << "Azinformation_path:" << Azinformation_path << std::endl;
  tinyxml2::XMLDocument xml;
  xml.LoadFile(Azinformation_path.c_str());
  tinyxml2::XMLElement *root = xml.RootElement();
  int node_id = 0;
  for (tinyxml2::XMLElement *az = root->FirstChildElement(); az != nullptr;
       az = az->NextSiblingElement()) {
    std::string az_id(az->Attribute("id"));
    std::string proxy(az->Attribute("proxy"));
    std::cout << "az_id: " << az_id << " , proxy: " << proxy << std::endl;
    m_AZ_info[std::stoi(az_id)].AZ_id = std::stoi(az_id);
    auto pos = proxy.find(':');
    m_AZ_info[std::stoi(az_id)].proxy_ip = proxy.substr(0, pos);
    m_AZ_info[std::stoi(az_id)].proxy_port =
        std::stoi(proxy.substr(pos + 1, proxy.size()));
    for (tinyxml2::XMLElement *node =
             az->FirstChildElement()->FirstChildElement();
         node != nullptr; node = node->NextSiblingElement()) {
      std::string node_uri(node->Attribute("uri"));
      std::cout << "____node: " << node_uri << std::endl;
      m_AZ_info[std::stoi(az_id)].nodes.push_back(node_id);
      m_Node_info[node_id].Node_id = node_id;
      auto pos = node_uri.find(':');
      m_Node_info[node_id].Node_ip = node_uri.substr(0, pos);
      m_Node_info[node_id].Node_port =
          std::stoi(node_uri.substr(pos + 1, node_uri.size()));
      m_Node_info[node_id].AZ_id = std::stoi(az_id);
      m_Node_info[node_id].storage = storages[node_id];
      m_Node_info[node_id].bandwidth = bandwidth[node_id];
      node_id++;
    }
  }
  return true;
}

bool check_merge(int g, std::vector<int> &a, std::vector<int> &b) {
  int group_a = a[1];
  int group_b = b[1];
  if (group_a == 0) {
    group_a = 1;
  }
  if (group_b == 0) {
    group_b = 1;
  }
  int room = group_a + group_b + g;
  int total = (a[0] + a[1] + a[2]) + (b[0] + b[1] + b[2]);
  if (room >= total) {
    return true;
  } else {
    return false;
  }
}

bool better_arrange(std::vector<int> &a, std::vector<int> &b) {
  return (a[0] + a[1] + a[2]) < (b[0] + b[1] + b[2]);
}

bool left_room_cmp(std::pair<int, int> &a, std::pair<int, int> &b) {
  return a.second > b.second;
}

std::vector<std::vector<int>> merge_tail(int k, int g, int b,
                                         std::vector<std::vector<int>> result) {
  std::sort(result.begin(), result.end(), better_arrange);
  int ending;
  for (int i = 0; i < result.size(); i++) {
    if (result[i][0] == g + 1) {
      ending = i;
      break;
    }
  }
  std::unordered_set<int> erase;
  for (int i = 0; i < ending; i++) {
    if (erase.find(i) != erase.end()) {
      continue;
    }
    for (int j = i + 1; j < ending; j++) {
      if (erase.find(j) != erase.end()) {
        continue;
      }
      if (check_merge(g, result[i], result[j])) {
        result[i][0] += result[j][0];
        result[i][1] += result[j][1];
        result[i][2] += result[j][2];
        erase.insert(j);
      }
    }
  }
  std::vector<std::vector<int>> back_up = result;
  result.clear();
  for (int i = 0; i < back_up.size(); i++) {
    if (erase.find(i) == erase.end()) {
      result.push_back(back_up[i]);
    }
  }
  return result;
}

// data local global
std::vector<std::vector<int>>
generate_placement_strategy_1(int k, int g, int b, bool merge = false) {
  //    cout << "k: " << k << ", g: " << g << ", b: " << b << endl;
  std::vector<std::vector<int>> result;
  result.push_back({0, 0, g});
  int sita = g / b;
  int left_data = k;
  if (sita >= 1) {
    while (left_data > 0) {
      if (left_data >= sita * b) {
        result.push_back({sita * b, sita, 0});
        left_data -= (sita * b);
      } else {
        int local =
            (left_data % b == 0) ? (left_data / b) : (left_data / b + 1);
        result.push_back({left_data, local, 0});
        left_data -= left_data;
      }
    }
  } else {
    int group = (k % b == 0) ? (k / b) : (k / b + 1);
    int i = 0;
    for (; i < group - 1; i++) {
      int temp_left_data = b;
      while (temp_left_data > 0) {
        if (temp_left_data > g + 1) {
          result.push_back({g + 1, 0, 0});
          temp_left_data -= (g + 1);
        } else if (temp_left_data == g + 1) {
          result.push_back({g + 1, 0, 0});
          result.push_back({0, 1, 0});
          temp_left_data -= (g + 1);
        } else {
          result.push_back({temp_left_data, 1, 0});
          temp_left_data -= temp_left_data;
        }
      }
      left_data -= b;
    }
    while (left_data > 0) {
      if (left_data > g + 1) {
        result.push_back({g + 1, 0, 0});
        left_data -= (g + 1);
      } else if (left_data == g + 1) {
        result.push_back({g + 1, 0, 0});
        result.push_back({0, 1, 0});
        left_data -= (g + 1);
      } else {
        result.push_back({left_data, 1, 0});
        left_data -= left_data;
      }
    }
  }
  if (merge) {
    result = merge_tail(k, g, b, result);
  }
  return result;
}

// data local global
std::vector<std::vector<int>>
generate_placement_strategy_2(int k, int g, int b,
                              bool repair_only_by_data = false) {
  std::vector<std::vector<int>> result;
  int sita = g / b;
  int left_data = k;
  if (sita >= 1) {
    while (left_data > 0) {
      if (left_data >= sita * b) {
        result.push_back({sita * b, sita, 0});
        left_data -= (sita * b);
      } else {
        int local =
            (left_data % b == 0) ? (left_data / b) : (left_data / b + 1);
        result.push_back({left_data, local, 0});
        left_data -= left_data;
      }
    }
  } else {
    int group = (k % b == 0) ? (k / b) : (k / b + 1);
    int i = 0;
    for (; i < group - 1; i++) {
      int temp_left_data = b;
      while (temp_left_data > 0) {
        if (temp_left_data > g + 1) {
          result.push_back({g + 1, 0, 0});
          temp_left_data -= (g + 1);
        } else if (temp_left_data == g + 1) {
          result.push_back({g + 1, 0, 0});
          result.push_back({0, 1, 0});
          temp_left_data -= (g + 1);
        } else {
          result.push_back({temp_left_data, 1, 0});
          temp_left_data -= temp_left_data;
        }
      }
      left_data -= b;
    }
    while (left_data > 0) {
      if (left_data > g + 1) {
        result.push_back({g + 1, 0, 0});
        left_data -= (g + 1);
      } else if (left_data == g + 1) {
        result.push_back({g + 1, 0, 0});
        result.push_back({0, 1, 0});
        left_data -= (g + 1);
      } else {
        result.push_back({left_data, 1, 0});
        left_data -= left_data;
      }
    }
  }
  if (!repair_only_by_data) {
    result = merge_tail(k, g, b, result);
  }
  std::vector<std::pair<int, int>> help;
  int sum_left_room = 0;
  for (int i = 0; i < result.size(); i++) {
    int group = result[i][1];
    if (result[i][1] == 0) {
      group = 1;
    }
    int max_room = g + group;
    int left_room = max_room - result[i][0] - result[i][1];
    help.push_back({i, left_room});
    sum_left_room += left_room;
  }

  int left_g = g;
  if (sum_left_room >= g) {
    std::sort(help.begin(), help.end(), left_room_cmp);
    for (int i = 0; i < help.size() && left_g > 0; i++) {
      if (help[i].second > 0) {
        int j = help[i].first;
        if (left_g >= help[i].second) {
          result[j][2] = help[i].second;
          left_g -= result[j][2];
        } else {
          result[j][2] = left_g;
          left_g -= left_g;
        }
      }
    }
    assert(left_g == 0);
  } else {
    result.push_back({0, 0, left_g});
  }
  return result;
}

bool cmp_both(std::pair<int, double> &a, std::pair<int, double> &b) {
  return a.second < b.second;
}

grpc::Status CoordinatorImpl::simulate_d_read(
    ::grpc::ServerContext *context,
    const coordinator_proto::d_read_para *d_read_para,
    coordinator_proto::d_read_result *d_read_result) {
  m_mutex.lock();
  ObjectItemBigSmall object_infro =
      m_object_table_big_small_commit.at(d_read_para->key());
  m_mutex.unlock();
  int faild_data_block_idx = dis(gen);
  do_repair(object_infro.stripes[0], {faild_data_block_idx});
  return grpc::Status::OK;
}

bool cmp_cost(std::pair<std::vector<int>, double> &a,
              std::pair<std::vector<int>, double> &b) {
  return a.second > b.second;
}

void CoordinatorImpl::generate_placement(
    std::vector<unsigned int> &stripe_nodes, int stripe_id) {
  StripeItem &stripe_info = m_Stripe_info[stripe_id];
  int k = stripe_info.k;
  int real_l = stripe_info.real_l;
  int full_group_l = (k % real_l != 0) ? (real_l - 1) : real_l;
  int g_m = stripe_info.g_m;
  int b = stripe_info.b;
  int tail_group = k - full_group_l * b;
  OppoProject::EncodeType encode_type = stripe_info.encodetype;
  OppoProject::PlacementType placement_type = m_encode_parameter.placementtype;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned int> dis(0, m_Node_info.size() - 1);

  if (encode_type == Azure_LRC) {
    if (placement_type == Random) {
      std::vector<bool> vis(m_Node_info.size(), false);
      std::vector<std::pair<std::unordered_set<int>, int>> help(
          m_AZ_info.size());
      for (int i = 0; i < int(m_AZ_info.size()); i++) {
        help[i].second = 0;
      }
      int node_idx, az_idx, area_upper;
      for (int i = 0; i < full_group_l; i++) {
        for (int j = 0; j < b; j++) {
          do {
            node_idx = dis(gen);
            az_idx = m_Node_info[node_idx].AZ_id;
            area_upper = g_m + help[az_idx].first.size();
          } while (vis[node_idx] == true || help[az_idx].second == area_upper);
          stripe_nodes.push_back(node_idx);
          m_Node_info[node_idx].stripes.insert(stripe_id);
          vis[node_idx] = true;
          help[az_idx].first.insert(i);
          help[az_idx].second++;
        }
      }
      for (int i = 0; i < tail_group; i++) {
        do {
          node_idx = dis(gen);
          az_idx = m_Node_info[node_idx].AZ_id;
          area_upper = g_m + help[az_idx].first.size();
        } while (vis[node_idx] == true || help[az_idx].second == area_upper);
        stripe_nodes.push_back(node_idx);
        m_Node_info[node_idx].stripes.insert(stripe_id);
        vis[node_idx] = true;
        help[az_idx].first.insert(full_group_l);
        help[az_idx].second++;
      }
      for (int i = 0; i < g_m; i++) {
        do {
          node_idx = dis(gen);
          az_idx = m_Node_info[node_idx].AZ_id;
          area_upper = g_m + help[az_idx].first.size();
        } while (vis[node_idx] == true || help[az_idx].second == area_upper);
        stripe_nodes.push_back(node_idx);
        m_Node_info[node_idx].stripes.insert(stripe_id);
        vis[node_idx] = true;
        help[az_idx].second++;
      }
      for (int i = 0; i < full_group_l; i++) {
        do {
          node_idx = dis(gen);
          az_idx = m_Node_info[node_idx].AZ_id;
          area_upper = g_m + help[az_idx].first.size();
        } while (vis[node_idx] == true || help[az_idx].second == area_upper);
        stripe_nodes.push_back(node_idx);
        m_Node_info[node_idx].stripes.insert(stripe_id);
        vis[node_idx] = true;
        if (help[az_idx].first.count(i) == 0) {
          help[az_idx].first.insert(i);
        }
        help[az_idx].second++;
      }
      if (tail_group > 0) {
        do {
          node_idx = dis(gen);
          az_idx = m_Node_info[node_idx].AZ_id;
          area_upper = g_m + help[az_idx].first.size();
        } while (vis[node_idx] == true || help[az_idx].second == area_upper);
        stripe_nodes.push_back(node_idx);
        m_Node_info[node_idx].stripes.insert(stripe_id);
        vis[node_idx] = true;
        if (help[az_idx].first.count(full_group_l) == 0) {
          help[az_idx].first.insert(full_group_l);
        }
        help[az_idx].second++;
      }

    } else if (placement_type == Par_1 ||
               placement_type == Par_2 ||
               placement_type == Par_2_random) {
      int num_nodes = tail_group > 0 ? (k + g_m + full_group_l + 1)
                                     : (k + g_m + full_group_l);
      stripe_nodes.resize(num_nodes);
      std::vector<std::vector<int>> result;
      if (placement_type == Par_1) {
        result = generate_placement_strategy_1(k, g_m, b);
      } else if (placement_type == Par_2 || Par_2_random) {
        result = generate_placement_strategy_2(k, g_m, b);
      }
      int data_idx = 0;
      int global_idx = k;
      int local_idx = k + g_m;
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<unsigned int> dis(0, m_AZ_info.size() - 1);
      std::unordered_set<int> used_az;
      for (int i = 0; i < result.size(); i++) {
        int az_idx;
        do {
          az_idx = dis(gen);
        } while (used_az.count(az_idx) > 0);
        used_az.insert(az_idx);
        AZitem &az = m_AZ_info[az_idx];
        std::random_device rd2;
        std::mt19937 gen2(rd2());
        std::uniform_int_distribution<unsigned int> dis2(0,
                                                         az.nodes.size() - 1);
        std::unordered_set<int> used_node;
        // data
        for (int j = 0; j < result[i][0]; j++) {
          int node_idx;
          do {
            node_idx = dis2(gen2);
          } while (used_node.count(node_idx) > 0);
          used_node.insert(node_idx);
          stripe_nodes[data_idx++] = az.nodes[node_idx];
          m_Node_info[az.nodes[node_idx]].stripes.insert(stripe_id);
        }
        // local
        for (int j = 0; j < result[i][1]; j++) {
          int node_idx;
          do {
            node_idx = dis2(gen2);
          } while (used_node.count(node_idx) > 0);
          used_node.insert(node_idx);
          stripe_nodes[local_idx++] = az.nodes[node_idx];
          m_Node_info[az.nodes[node_idx]].stripes.insert(stripe_id);
        }
        // global
        for (int j = 0; j < result[i][2]; j++) {
          int node_idx;
          do {
            node_idx = dis2(gen2);
          } while (used_node.count(node_idx) > 0);
          used_node.insert(node_idx);
          stripe_nodes[global_idx++] = az.nodes[node_idx];
          m_Node_info[az.nodes[node_idx]].stripes.insert(stripe_id);
        }
      }
    } else if (placement_type == Par_2_load) {
      int num_nodes = tail_group > 0 ? (k + g_m + full_group_l + 1)
                                     : (k + g_m + full_group_l);
      stripe_nodes.resize(num_nodes);
      std::vector<std::vector<int>> result;
      result = generate_placement_strategy_2(k, g_m, b);
      std::vector<double> num_of_blocks_each_par;
      std::vector<double> num_of_data_blocks_each_par;
      for (auto &par : result) {
        num_of_blocks_each_par.push_back(par[0] + par[1] + par[2]);
        num_of_data_blocks_each_par.push_back(par[0]);
      }
      double avg_blocks = 0;
      double avg_data_blocks = 0;
      for (int i = 0; i < num_of_blocks_each_par.size(); i++) {
        avg_blocks += num_of_blocks_each_par[i];
        avg_data_blocks += num_of_data_blocks_each_par[i];
      }
      avg_blocks = avg_blocks / (double)num_of_blocks_each_par.size();
      avg_data_blocks = avg_data_blocks / (double)num_of_blocks_each_par.size();
      std::vector<std::pair<std::vector<int>, double>> cost_each_par;
      for (int i = 0; i < num_of_blocks_each_par.size(); i++) {
        double storage_cost = num_of_blocks_each_par[i] / avg_blocks;
        double network_cost = num_of_data_blocks_each_par[i] / avg_data_blocks;
        double cost = storage_cost * (1 - alpha) + network_cost * alpha;
        cost_each_par.push_back({result[i], cost});
      }
      std::sort(cost_each_par.begin(), cost_each_par.end(), cmp_cost);
      result.clear();
      for (auto &p : cost_each_par) {
        result.push_back(p.first);
      }
      int data_idx = 0;
      int global_idx = k;
      int local_idx = k + g_m;
      double node_avg_storage_cost, node_avg_network_cost;
      double az_avg_storage_cost, az_avg_network_cost;
      compute_avg(node_avg_storage_cost, node_avg_network_cost,
                  az_avg_storage_cost, az_avg_network_cost);
      std::vector<std::pair<int, double>> az_sort_both;
      int az_idx = 0;
      for (auto &az : m_AZ_info) {
        double storage_cost, network_cost;
        compute_cost_for_az(az.second, storage_cost, network_cost);
        double temp = (storage_cost / az_avg_storage_cost) * (1 - alpha) +
                      (network_cost / az_avg_network_cost) * alpha;
        az_sort_both.push_back({az.first, temp});
      }
      std::sort(az_sort_both.begin(), az_sort_both.end(), cmp_both);
      for (int i = 0; i < result.size(); i++) {
        int az_id = az_sort_both[az_idx++].first;
        AZitem &az = m_AZ_info[az_id];
        std::vector<std::pair<int, double>> node_sort_both;
        for (auto &node_id : az.nodes) {
          Nodeitem &node_info = m_Node_info[node_id];
          double storage_cost = node_info.storage_cost / node_info.storage;
          double network_cost = node_info.network_cost / node_info.bandwidth;
          double temp = (storage_cost / node_avg_storage_cost) * (1 - alpha) +
                        (network_cost / node_avg_network_cost) * alpha;
          node_sort_both.push_back({node_id, temp});
        }
        std::sort(node_sort_both.begin(), node_sort_both.end(), cmp_both);
        int node_idx = 0;
        // data
        for (int j = 0; j < result[i][0]; j++) {
          int node_id = node_sort_both[node_idx++].first;
          stripe_nodes[data_idx++] = node_id;
          m_Node_info[node_id].stripes.insert(stripe_id);
        }
        // local
        for (int j = 0; j < result[i][1]; j++) {
          int node_id = node_sort_both[node_idx++].first;
          stripe_nodes[local_idx++] = node_id;
          m_Node_info[node_id].stripes.insert(stripe_id);
        }
        // global
        for (int j = 0; j < result[i][2]; j++) {
          int node_id = node_sort_both[node_idx++].first;
          stripe_nodes[global_idx++] = node_id;
          m_Node_info[node_id].stripes.insert(stripe_id);
        }
      }
    }
  }
  for (int i = 0; i < full_group_l; i++) {
    for (int j = 0; j < b; j++) {
      std::cout << stripe_nodes[i * b + j] << " ";
    }
    std::cout << std::endl;
  }
  if (tail_group > 0) {
    for (int i = 0; i < tail_group; i++) {
      std::cout << stripe_nodes[full_group_l * b + i] << " ";
    }
    std::cout << std::endl;
  }
  for (int i = 0; i < g_m; i++) {
    std::cout << stripe_nodes[k + i] << " ";
  }
  std::cout << std::endl;
  int real_l_include_gl = tail_group > 0 ? (full_group_l + 1) : (full_group_l);
  for (int i = 0; i < real_l_include_gl; i++) {
    std::cout << stripe_nodes[k + g_m + i] << " ";
  }
  std::cout << std::endl;
  std::cout << "******************************" << std::endl;
  return;
}

} // namespace OppoProject
