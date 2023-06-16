#include "coordinator.h"
#include <sys/stat.h>

int main(int argc, char **argv) {
  pid_t pid = fork();
  if (pid > 0) {
    exit(0);
  }
  setsid();
  umask(0);
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  /*此处需要补充*/
  char buff[256];
  getcwd(buff, 256);
  std::string config_path =
      std::string(buff) + "/../../config/AZInformation.xml";
  std::cout << "Current working directory: " << config_path << std::endl;
  OppoProject::Coordinator coordinator("10.0.0.10:55555", config_path);
  coordinator.Run();
  return 0;
}