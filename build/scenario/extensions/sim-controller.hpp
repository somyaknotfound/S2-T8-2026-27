/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Global state for one simulation run: cache contents per node, the answer/forward decision,
// request sequencing across clients, and the per-request CSV log.

#ifndef SIMCACHE_SIM_CONTROLLER_HPP
#define SIMCACHE_SIM_CONTROLLER_HPP

#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ns3 {
namespace simcache {

class SimConsumer;

// When cache contents are (re)drawn as a uniform random B-subset of the catalogue:
//  PerAttempt - at every Interest arrival (the base paper's independence assumption)
//  PerRequest - once per request, persisting across its retries
//  Static     - once per run
enum class Placement { PerAttempt, PerRequest, Static };

struct SimConfig
{
  uint32_t catalog = 100;
  uint32_t cacheSize = 10;
  double alpha = 0.5;
  double threshold = 1.0;
  Placement placement = Placement::Static;
  uint32_t requests = 1000;
  uint32_t maxAttempts = 5000;
  Time delta = MilliSeconds(10);
  Time interestLifetime = Seconds(2);
  uint32_t run = 1;
  std::string outFile = "results.csv";
};

struct RequestRecord
{
  uint32_t requestId = 0;
  uint32_t clientNode = 0;
  uint32_t item = 0;
  uint32_t attempts = 0;
  uint32_t timeouts = 0;
  double delayMs = 0;
  uint32_t servedNode = 0;
  double similarity = 0;
  bool censored = false;
};

class SimController
{
public:
  static SimController&
  Get();

  void
  Configure(const SimConfig& config);

  const SimConfig&
  GetConfig() const
  {
    return m_config;
  }

  // position = hop index along the path toward the origin (0 = first cache)
  void
  AddNode(uint32_t nodeId, uint32_t position, bool isCache);

  void
  AddClient(Ptr<SimConsumer> consumer, uint32_t nodeId);

  bool
  IsCache(uint32_t nodeId) const;

  double
  Similarity(uint32_t a, uint32_t b) const;

  // most similar cached item and its similarity
  std::pair<uint32_t, double>
  BestMatch(uint32_t nodeId, uint32_t item);

  bool
  ShouldAnswer(uint32_t nodeId, double similarity);

  void
  Start(Time at);

  void
  OnRequestDone(const RequestRecord& record);

private:
  SimController() = default;

  void
  DrawContents(uint32_t nodeId);

  void
  StartNextRequest();

  void
  WriteRecord(const RequestRecord& record);

private:
  SimConfig m_config;
  std::map<uint32_t, std::vector<uint32_t>> m_contents;
  std::map<uint32_t, uint32_t> m_position;
  std::vector<std::pair<Ptr<SimConsumer>, uint32_t>> m_clients;
  Ptr<UniformRandomVariable> m_rand;
  uint32_t m_started = 0;
  uint32_t m_done = 0;
  std::ofstream m_out;
};

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_SIM_CONTROLLER_HPP
