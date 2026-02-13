#ifndef METADATA_HOLDER_H
#define METADATA_HOLDER_H

#include <map>
#include <string>

class MetadataHolder {
public:
  void setMetadata(const std::string &key, const std::string &value) {
    _metadata[key] = value;
  }

  std::string getMetadata(const std::string &key) const {
    auto it = _metadata.find(key);
    return (it != _metadata.end()) ? it->second : std::string();
  }

  bool hasMetadata(const std::string &key) const {
    return _metadata.find(key) != _metadata.end();
  }

private:
  std::map<std::string, std::string> _metadata;
};

#endif // METADATA_HOLDER_H
