/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "sim-controller.hpp"
#include "sim-consumer.hpp"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>

NS_LOG_COMPONENT_DEFINE("simcache.SimController");

namespace ns3 {
namespace simcache {

static const char*
PlacementName(Placement placement)
{
  switch (placement) {
  case Placement::PerAttempt:
    return "per-attempt";
  case Placement::PerRequest:
    return "per-request";
  case Placement::Static:
    return "static";
  }
  return "?";
}

SimController&
SimController::Get()
{
  static SimController instance;
  return instance;
}

void
SimController::Configure(const SimConfig& config)
{
  NS_ABORT_MSG_IF(config.cacheSize > config.catalog, "cache size larger than catalogue");
  m_config = config;
  m_rand = CreateObject<UniformRandomVariable>();

  m_out.open(m_config.outFile);
  NS_ABORT_MSG_UNLESS(m_out.is_open(), "cannot open " << m_config.outFile);
  m_out << "run,alpha,threshold,placement,catalog,cache_size,delta_ms,request,client_node,"
           "client_pos,item,attempts,timeouts,delay_ms,served_node,served_hop,similarity,"
           "censored\n";
}

void
SimController::AddNode(uint32_t nodeId, uint32_t position, bool isCache)
{
  m_position[nodeId] = position;
  if (isCache) {
    m_contents[nodeId] = {};
  }
}

void
SimController::AddClient(Ptr<SimConsumer> consumer, uint32_t nodeId)
{
  m_clients.emplace_back(consumer, nodeId);
}

bool
SimController::IsCache(uint32_t nodeId) const
{
  return m_contents.count(nodeId) > 0;
}

double
SimController::Similarity(uint32_t a, uint32_t b) const
{
  // synthetic dataset of the base paper: s = 1 - |a - b| / (C - 1), gamma = 1
  double distance = std::abs(static_cast<double>(a) - static_cast<double>(b));
  return 1.0 - distance / (m_config.catalog - 1);
}

void
SimController::DrawContents(uint32_t nodeId)
{
  // uniform random B-subset via a partial Fisher-Yates shuffle
  std::vector<uint32_t> all(m_config.catalog);
  std::iota(all.begin(), all.end(), 0);
  for (uint32_t i = 0; i < m_config.cacheSize; ++i) {
    uint32_t j = m_rand->GetInteger(i, m_config.catalog - 1);
    std::swap(all[i], all[j]);
  }
  m_contents[nodeId].assign(all.begin(), all.begin() + m_config.cacheSize);
}

std::pair<uint32_t, double>
SimController::BestMatch(uint32_t nodeId, uint32_t item)
{
  if (m_config.placement == Placement::PerAttempt) {
    DrawContents(nodeId);
  }

  const auto& contents = m_contents.at(nodeId);
  uint32_t best = contents.front();
  double bestSimilarity = Similarity(item, best);
  for (uint32_t candidate : contents) {
    double s = Similarity(item, candidate);
    if (s > bestSimilarity) {
      best = candidate;
      bestSimilarity = s;
    }
  }
  return {best, bestSimilarity};
}

bool
SimController::ShouldAnswer(uint32_t /*nodeId*/, double /*similarity*/)
{
  // blind policy of the base paper: answer with probability alpha, even on an exact hit
  return m_rand->GetValue() < m_config.alpha;
}

void
SimController::Start(Time at)
{
  NS_ABORT_MSG_IF(m_clients.empty(), "no clients registered");
  for (auto& entry : m_contents) {
    DrawContents(entry.first);
  }
  Simulator::Schedule(at, &SimController::StartNextRequest, this);
}

void
SimController::StartNextRequest()
{
  if (m_config.placement == Placement::PerRequest) {
    for (auto& entry : m_contents) {
      DrawContents(entry.first);
    }
  }

  uint32_t requestId = ++m_started;
  auto& client = m_clients[m_rand->GetInteger(0, m_clients.size() - 1)];
  uint32_t item = m_rand->GetInteger(0, m_config.catalog - 1);

  // the client's events must run in its own node context: strategies use it to find the node
  Simulator::ScheduleWithContext(client.second, Seconds(0), &SimConsumer::StartRequest,
                                 client.first, requestId, item);
}

void
SimController::OnRequestDone(const RequestRecord& record)
{
  WriteRecord(record);
  if (++m_done >= m_config.requests) {
    m_out.close();
    Simulator::Stop();
    return;
  }
  Simulator::ScheduleNow(&SimController::StartNextRequest, this);
}

void
SimController::WriteRecord(const RequestRecord& r)
{
  uint32_t clientPos = m_position.at(r.clientNode);
  int servedHop = static_cast<int>(m_position.at(r.servedNode)) - static_cast<int>(clientPos);

  m_out << m_config.run << ',' << m_config.alpha << ',' << m_config.threshold << ','
        << PlacementName(m_config.placement) << ',' << m_config.catalog << ','
        << m_config.cacheSize << ',' << m_config.delta.GetMilliSeconds() << ',' << r.requestId
        << ',' << r.clientNode << ',' << clientPos << ',' << r.item << ',' << r.attempts << ','
        << r.timeouts << ',' << std::setprecision(10) << r.delayMs << ',' << r.servedNode << ','
        << servedHop << ',' << r.similarity << ',' << (r.censored ? 1 : 0) << '\n';
}

} // namespace simcache
} // namespace ns3
