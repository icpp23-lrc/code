#include "client.h"
#include "toolbox.h"
#include <chrono>
#include <fstream>
using namespace std;
using namespace chrono;

int main(int argc, char **argv) {
  if (argc != 6) {
    std::cout << "./run_client Random 3 -1 2 1" << std::endl;
    exit(-1);
  }

  OppoProject::PlacementType placement_type;
  if (std::string(argv[1]) == "Random") {
    placement_type = OppoProject::Random;
  } else if (std::string(argv[1]) == "Best_Placement") {
    placement_type = OppoProject::Best_Placement;
  } else if (std::string(argv[1]) == "Best_Best_Placement") {
    placement_type = OppoProject::Best_Best_Placement;
  } else if (std::string(argv[1]) == "Best_Best_Best_Placement") {
    placement_type = OppoProject::Best_Best_Best_Placement;
  } else {
    std::cout << "error: unknown placement_type" << std::endl;
    exit(-1);
  }
  int k = std::stoi(std::string(argv[2]));
  int real_l = std::stoi(std::string(argv[3]));
  int b = std::ceil((double)k / (double)real_l);
  int g_m = std::stoi(std::string(argv[4]));
  int block_size = std::stoi(std::string(argv[5])); //单位KB
  int num_of_nodes = 200;
  int value_length = block_size * 1024 * k;
  int num_of_stripes = 0.25 * 1024 * 1024 / (value_length / 1024);
  int num_of_times = 15;
  std::cout << num_of_stripes << " " << block_size << " " << value_length
            << std::endl;

  OppoProject::Client client(std::string("10.0.0.10"), 44444,
                             std::string("10.0.0.10:55555"));
  std::cout << client.sayHelloToCoordinatorByGrpc("MMMMMMMM") << std::endl;
  if (client.SetParameterByGrpc({false, OppoProject::Azure_LRC, placement_type,
                                 k, real_l, g_m, b, 0, 2147483647})) {
    std::cout << "set parameter successfully!" << std::endl;
  } else {
    std::cout << "Failed to set parameter!" << std::endl;
  }

  // close(STDOUT_FILENO);
  // close(STDERR_FILENO);
  std::unordered_map<std::string, std::string> key_values;
  std::unordered_set<std::string> keys;
  for (int i = 0; i < num_of_stripes; i++) {
    std::cout << "progress: " << i << std::endl;
    std::string key;
    std::string value;
    OppoProject::gen_key_value(keys, 50, key, value_length, value);
    key_values[key] = value;
    keys.insert(key);
    client.set(key, value, "00");
  }
  std::cout << "开始修复" << std::endl;
  auto start = system_clock::now();
  for (int times = 0; times < num_of_times; times++) {
    std::cout << "round ********** " << times << std::endl;
    for (int j = 0; j < num_of_nodes; j++) {
      int temp = j;
      std::cout << "repair: " << temp << std::endl;
      std::vector<int> failed_node_list = {temp};
      client.repair(failed_node_list);
    }
  }
  auto end = system_clock::now();
  auto duration = duration_cast<microseconds>(end - start);
  string test_result_file = "test.result";
  ofstream fout(test_result_file, std::ios::app);
  for (auto kv : key_values) {
    std::string temp;
    client.get(kv.first, temp);
    if (temp != kv.second) {
      fout << "repair fail" << std::endl;
      break;
    } else {
      std::cout << "repair success" << std::endl;
    }
  }
  double node_storage_bias;
  double node_network_bias;
  double az_storage_bias;
  double az_network_bias;
  double cross_repair_traffic;
  double degraded_time;
  double all_time;
  client.checkBias(node_storage_bias, node_network_bias, az_storage_bias,
                   az_network_bias, cross_repair_traffic, degraded_time,
                   all_time);
  double time_cost = double(duration.count()) * microseconds::period::num /
                     microseconds::period::den;
  fout << "时间开销：" << time_cost / num_of_times << "秒" << endl;
  double repair_speed = (double)num_of_stripes * (double)(k + real_l + g_m) *
                        (double)((double)block_size / 1024) / all_time;
  fout << "修复速度：" << repair_speed * num_of_times << "MB/秒" << endl;
  fout << "node_storage_bias: " << node_storage_bias << std::endl;
  fout << "node_network_bias: " << node_network_bias << std::endl;
  fout << "az_storage_bias: " << az_storage_bias << std::endl;
  fout << "az_network_bias: " << az_network_bias << std::endl;
  fout << "cross_repair_traffic: " << cross_repair_traffic << std::endl;
  fout << "degraded_time: " << degraded_time / num_of_times << std::endl;
  fout << "all_time: " << all_time / num_of_times << std::endl;
  fout.close();
}
