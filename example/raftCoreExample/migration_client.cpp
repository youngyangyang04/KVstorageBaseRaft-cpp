#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

#include "clerk.h"
#include "shardConfig.h"

namespace {
void ShowArgsHelp() {
  std::cout << "format: migration_client -f <configFile> -o <put|get|append> -k <key> [-v <value>]" << std::endl;
}
}  // namespace

int main(int argc, char** argv) {
  std::string configFileName = "test.conf";
  std::string op = "get";
  std::string key;
  std::string value;

  int c = 0;
  while ((c = getopt(argc, argv, "f:o:k:v:h")) != -1) {
    switch (c) {
      case 'f':
        configFileName = optarg;
        break;
      case 'o':
        op = optarg;
        break;
      case 'k':
        key = optarg;
        break;
      case 'v':
        value = optarg;
        break;
      case 'h':
      default:
        ShowArgsHelp();
        return 0;
    }
  }

  if (key.empty()) {
    ShowArgsHelp();
    return EXIT_FAILURE;
  }

  Clerk client;
  client.Init(configFileName);

  if (op == "put") {
    client.Put(key, value);
    std::cout << "OK put key=" << key << " value=" << value << std::endl;
    return 0;
  }

  if (op == "append") {
    client.Append(key, value);
    std::cout << "OK append key=" << key << " value=" << value << std::endl;
    return 0;
  }

  if (op == "get") {
    const std::string readValue = client.Get(key);
    std::cout << "VALUE " << key << "=" << readValue << std::endl;
    return 0;
  }

  ShowArgsHelp();
  return EXIT_FAILURE;
}
