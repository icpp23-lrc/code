#ifndef CLIENT_H
#define CLIENT_H

#ifdef BAZEL_BUILD
#include "src/proto/coordinator.grpc.pb.h"
#else
#include "coordinator.grpc.pb.h"
#endif

#include "meta_definition.h"
#include <grpcpp/grpcpp.h>
#include <asio.hpp>
namespace OppoProject
{
  class Client
  {
  public:
    Client(std::string ClientIP, int ClientPort, std::string CoordinatorIpPort) : m_coordinatorIpPort(CoordinatorIpPort),
                                                                                  m_clientIPForGet(ClientIP),
                                                                                  m_clientPortForGet(ClientPort),
                                                                                  acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::address::from_string(ClientIP.c_str()), m_clientPortForGet))
    {
      auto channel = grpc::CreateChannel(m_coordinatorIpPort, grpc::InsecureChannelCredentials());
      m_coordinator_ptr = coordinator_proto::CoordinatorService::NewStub(channel);
    }
    std::string sayHelloToCoordinatorByGrpc(std::string hello);
    bool set(std::string key, std::string value, std::string flag);
    bool SetParameterByGrpc(ECSchema input_ecschema, int alpha = 50);
    bool get(std::string key, std::string &value);
    bool repair(std::vector<int> failed_node_list);

    // update
    bool update(std::string key, int offset, int length);
    bool checkBias(double &node_storage_bias, double &node_network_bias,
                   double &az_storage_bias, double &az_network_bias,
                   double &cross_repair_traffic, double &degraded_time,
                   double &all_time) {
      grpc::ClientContext context;
      coordinator_proto::myVoid request;
      coordinator_proto::checkBiasResult reply;
      grpc::Status status = m_coordinator_ptr->checkBias(&context, request, &reply);
      node_storage_bias = reply.node_storage_bias();
      node_network_bias = reply.node_network_bias();
      az_storage_bias = reply.az_storage_bias();
      az_network_bias = reply.az_network_bias();
      cross_repair_traffic = reply.cross_repair_traffic();
      degraded_time = reply.degraded_time();
      all_time = reply.all_time();
      return status.ok();
    }
    bool simulate_d_read(std::string key, std::string &value);

  private:
    std::unique_ptr<coordinator_proto::CoordinatorService::Stub> m_coordinator_ptr;
    std::string m_coordinatorIpPort;
    std::string m_clientIPForGet;
    int m_clientPortForGet;
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor;
  };

} // namespace OppoProject

#endif // CLIENT_H