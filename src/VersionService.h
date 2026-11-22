#ifndef VersionService_h
#define VersionService_h

#include <HttpEndpoint.h>
#include <StatefulService.h>
#include "version.h"

#define VERSION_ENDPOINT_PATH "/rest/version"

class VersionInfo {
 public:
  String version;
  String apiVersion;
  String buildDate;
  String buildTime;
  String projectName;
  String projectUrl;

  static void read(VersionInfo& info, JsonObject& root) {
    root["version"] = info.version;
    root["api_version"] = info.apiVersion;
    root["build_date"] = info.buildDate;
    root["build_time"] = info.buildTime;
    root["project_name"] = info.projectName;
    root["project_url"] = info.projectUrl;
  }

  static StateUpdateResult update(JsonObject& root, VersionInfo& info) {
    // Version info is read-only
    return StateUpdateResult::UNCHANGED;
  }
};

class VersionService : public StatefulService<VersionInfo> {
 public:
  VersionService(AsyncWebServer* server, SecurityManager* securityManager);
  void begin();

 private:
  HttpEndpoint<VersionInfo> _httpEndpoint;
};

#endif
