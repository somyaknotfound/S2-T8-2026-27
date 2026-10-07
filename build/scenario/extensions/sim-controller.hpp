/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Global state for one simulation run: cache contents per node, the answer/forward decision,
// request sequencing across clients, and the per-request CSV log.

#ifndef SIMCACHE_SIM_CONTROLLER_HPP
#define SIMCACHE_SIM_CONTROLLER_HPP

#include "delay-aware.hpp"

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
struct RequestParams;

// When cache contents are (re)drawn as a uniform random B-subset of the catalogue:
//  PerAttempt - at every Interest arrival (the base paper's independence assumption)
//  PerRequest - once per request, persisting across its retries
//  Static     - once per run
enum class Placement { PerAttempt, PerRequest, Static };

// When a cache answers (each also needs a coin flip below alpha):
//  Blind     - always (the base paper; the cache never learns the client's threshold)
//  Threshold - only if its best match reaches the s_min carried in the Interest
//  Exclusion - only if it has not already served this request a rejected answer
//  DelayAware - no coin: exclusion, plus answer only if a rejection is expected to cost less
//               than going upstream (delay-aware.hpp); it never sees the user's threshold
enum class Policy { Blind, Threshold, Exclusion, DelayAware };

struct SimConfig
{
  uint32_t catalog = 100;
  uint32_t cacheSize = 10;
  double alpha = 0.5;
  double threshold = 1.0;
  std::vector<double> thresholds; // if set, each request draws its user's s from these
  Policy policy = Policy::Blind;
  Placement placement = Placement::Static;
  uint32_t requests = 1000;
  uint32_t maxAttempts = 5000;
  Time delta = MilliSeconds(10);
  Time interestLifetime = Seconds(2);
  uint32_t run = 1;
  std::string outFile = "results.csv";
  // scenario values copied into every CSV row, so merged runs stay self-describing
  std::string linkRate = "1Gbps";
  std::string bottleneckRate; // the last cache-to-origin link; empty = linkRate
  uint32_t queuePackets = 1000;
  double crossRate = 0;
  double lossPgb = 0;
  double lossPbg = 0;
  AwareConfig aware;
  std::string nodeOutFile;    // optional per-cache learning trace (delay-aware)
  uint32_t nodeLogEvery = 100; // requests between trace rows
};

struct RequestRecord
{
  uint32_t requestId = 0;
  uint32_t clientNode = 0;
  uint32_t item = 0;
  double threshold = 0;
  uint32_t attempts = 0;
  uint32_t timeouts = 0;
  double delayMs = 0;
  uint32_t servedNode = 0;
  uint32_t interestHops = 0; // links climbed by all answered attempts
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

  uint32_t
  Position(uint32_t nodeId) const;

  double
  Similarity(uint32_t a, uint32_t b) const;

  // most similar cached item and its similarity
  std::pair<uint32_t, double>
  BestMatch(uint32_t nodeId, uint32_t item);

  bool
  ShouldAnswer(uint32_t nodeId, double similarity, const RequestParams& params);

  // upstream measurements of a delay-aware cache; no-ops under the other policies
  void
  OnForwarded(uint32_t nodeId, const ::ndn::Name& name, int hopLimit);

  void
  OnUpstreamData(uint32_t nodeId, const ::ndn::Name& name, int answerHopLimit);

  void
  OnUpstreamExpired(uint32_t nodeId, const ::ndn::Name& name);

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

  void
  WriteNodeStates();

private:
  SimConfig m_config;
  std::map<uint32_t, std::vector<uint32_t>> m_contents;
  std::map<uint32_t, DelayAwareState> m_aware;
  std::map<uint32_t, uint32_t> m_position;
  std::vector<std::pair<Ptr<SimConsumer>, uint32_t>> m_clients;
  Ptr<UniformRandomVariable> m_rand;
  uint32_t m_started = 0;
  uint32_t m_done = 0;
  std::ofstream m_out;
  std::ofstream m_nodeOut;
};

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_SIM_CONTROLLER_HPP
