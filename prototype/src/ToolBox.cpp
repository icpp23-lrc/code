#include "toolbox.h"
#include <cstring>
#include <ctime>
#include <random>
#include <unordered_set>

bool OppoProject::random_generate_kv(std::string &key, std::string &value,
                                     int key_length, int value_length) {

  struct timespec tp;

  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
  srand(tp.tv_nsec);
  if (key_length == 0) {
  } else {
    for (int i = 0; i < key_length; i++) {
      key = key + char('a' + rand() % 26);
    }
  }
  if (value_length == 0) {
  } else {
    for (int i = 0; i < value_length / 26; i++) {
      for (int j = 65; j <= 90; j++) {
        value = value + char(j);
      }
    }
    for (int i = 0; i < value_length - int(value.size()); i++) {
      value = value + char('A' + i);
    }
  }
  return true;
}

std::vector<unsigned char> OppoProject::int_to_bytes(int integer) {
  std::vector<unsigned char> bytes(sizeof(int));
  unsigned char *p = (unsigned char *)(&integer);
  for (int i = 0; i < int(bytes.size()); i++) {
    memcpy(&bytes[i], p + i, 1);
  }
  return bytes;
}

int OppoProject::bytes_to_int(std::vector<unsigned char> &bytes) {
  int integer;
  unsigned char *p = (unsigned char *)(&integer);
  for (int i = 0; i < int(bytes.size()); i++) {
    memcpy(p + i, &bytes[i], 1);
  }
  return integer;
}

bool OppoProject::random_generate_value(std::string &value, int value_length) {
  /*生成一个固定大小的随机value*/
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned int> dis(0, 25);
  for (int i = 0; i < value_length; i++) {
    value = value + (dis(gen) % 2 ? char('a' + dis(gen) % 26)
                                  : char('A' + dis(gen) % 26));
  }
  return true;
}
void OppoProject::gen_key_value(std::unordered_set<std::string> keys,
                                int key_len, std::string &key, int value_len,
                                std::string &value) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned int> dis(0, 25);
  do {
    key.clear();
    for (int i = 0; i < key_len; i++) {
      key = key + (dis(gen) % 2 ? char('a' + dis(gen) % 26)
                                : char('A' + dis(gen) % 26));
    }
  } while (keys.count(key) > 0);
  for (int i = 0; i < value_len; i++) {
    value +=
        (dis(gen) % 2 ? char('a' + dis(gen) % 26) : char('A' + dis(gen) % 26));
  }
}
// namespace OppoProject