#ifndef COORDINATOR_H
#define COORDINATOR_H
#include "coordinator.grpc.pb.h"
#include "proxy.grpc.pb.h"
#include <condition_variable>
#include <grpc++/create_channel.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <meta_definition.h>
#include <mutex>
#include <thread>

namespace OppoProject {
class CoordinatorImpl final
    : public coordinator_proto::CoordinatorService::Service {
public:
  CoordinatorImpl() : cur_az(0), gen(rd()), dis(0, 7) {}
  grpc::Status setParameter(
      ::grpc::ServerContext *context,
      const coordinator_proto::Parameter *parameter,
      coordinator_proto::RepIfSetParaSucess *setParameterReply) override;
  grpc::Status sayHelloToCoordinator(
      ::grpc::ServerContext *context,
      const coordinator_proto::RequestToCoordinator *helloRequestToCoordinator,
      coordinator_proto::ReplyFromCoordinator *helloReplyFromCoordinator)
      override;
  grpc::Status uploadOriginKeyValue(
      grpc::ServerContext *context,
      const coordinator_proto::RequestProxyIPPort *keyValueSize,
      coordinator_proto::ReplyProxyIPPort *proxyIPPort) override;

  grpc::Status checkalive(
      grpc::ServerContext *context,
      const coordinator_proto::RequestToCoordinator *helloRequestToCoordinator,
      coordinator_proto::ReplyFromCoordinator *helloReplyFromCoordinator)
      override;
  grpc::Status
  reportCommitAbort(grpc::ServerContext *context,
                    const coordinator_proto::CommitAbortKey *commit_abortkey,
                    coordinator_proto::ReplyFromCoordinator
                        *helloReplyFromCoordinator) override;
  grpc::Status
  requestRepair(::grpc::ServerContext *context,
                const coordinator_proto::FailNodes *failed_node_list,
                coordinator_proto::RepIfRepairSucess *reply) override;
  grpc::Status
  checkCommitAbort(grpc::ServerContext *context,
                   const coordinator_proto::AskIfSetSucess *key,
                   coordinator_proto::RepIfSetSucess *reply) override;
  grpc::Status
  getValue(::grpc::ServerContext *context,
           const coordinator_proto::KeyAndClientIP *keyClient,
           coordinator_proto::RepIfGetSucess *getReplyClient) override;
  grpc::Status
  simulate_d_read(::grpc::ServerContext *context,
                  const coordinator_proto::d_read_para *d_read_para,
                  coordinator_proto::d_read_result *d_read_result) override;
  bool init_AZinformation(std::string Azinformation_path);
  bool init_proxy(std::string proxy_information_path);
  void generate_placement(std::vector<unsigned int> &stripe_nodes,
                          int stripe_id);
  void do_repair(int stripe_id, std::vector<int> failed_shard_idxs);
  void generate_repair_plan(
      int stripe_id, bool one_shard, std::vector<int> &failed_shard_idxs,
      std::vector<std::vector<std::pair<std::pair<std::string, int>, int>>>
          &shards_to_read,
      std::vector<int> &repair_span_az,
      std::vector<std::pair<int, int>> &new_locations_with_shard_idx,
      std::unordered_map<int, bool> &merge);

  grpc::Status checkBias(::grpc::ServerContext *context,
                         const ::coordinator_proto::myVoid *request,
                         ::coordinator_proto::checkBiasResult *response);
  void compute_cost_for_az(AZitem &az, double &storage_cost,
                           double &network_cost);
  void compute_node_bias(double &storage_bias, double &network_bias);
  void compute_az_bias(double &storage_bias, double &network_bias);
  void compute_avg(double &node_avg_storage_cost, double &node_avg_network_cost,
                   double &az_avg_storage_cost, double &az_avg_network_cost);

private:
  std::mutex m_mutex;
  int m_next_stripe_id = 0;
  int m_next_update_opration_id = 0;
  std::map<std::string, std::unique_ptr<proxy_proto::proxyService::Stub>>
      m_proxy_ptrs;
  std::unordered_map<std::string, ObjectItemBigSmall>
      m_object_table_big_small_updating;
  std::unordered_map<std::string, ObjectItemBigSmall>
      m_object_table_big_small_commit;
  ECSchema m_encode_parameter;
  std::map<unsigned int, AZitem> m_AZ_info;
  std::map<unsigned int, Nodeitem> m_Node_info;
  std::map<unsigned int, StripeItem> m_Stripe_info;
  int cur_az;
  std::condition_variable cv;
  /*for small object*/
  std::vector<int> buf_rest;
  StripeItem cur_stripe;
  int az_id_for_cur_stripe;

  // update
  std::map<unsigned int, std::vector<ShardidxRange>>
  split_update_length(std::string key, int update_offset_infile,
                      int update_length);
  double alpha = 0.5;
  int cross_repair_traffic = 0;
  double degraded_time = 0;
  double all_time = 0;

  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<unsigned int> dis;
};

class Coordinator {
public:
  Coordinator(std::string m_coordinator_ip_port,
              std::string m_Azinformation_path)
      : m_coordinator_ip_port{m_coordinator_ip_port},
        m_Azinformation_path{m_Azinformation_path} {
    m_coordinatorImpl.init_AZinformation(m_Azinformation_path);
    m_coordinatorImpl.init_proxy(m_Azinformation_path);
  };
  // Coordinator
  void Run() {
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    std::string server_address(m_coordinator_ip_port);
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&m_coordinatorImpl);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
  }

private:
  std::string m_coordinator_ip_port;
  std::string m_Azinformation_path;
  OppoProject::CoordinatorImpl m_coordinatorImpl;
};
} // namespace OppoProject

#endif