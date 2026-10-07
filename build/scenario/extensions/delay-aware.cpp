/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "delay-aware.hpp"

#include <algorithm>
#include <cmath>

namespace ns3 {
namespace simcache {

DelayAwareState::DelayAwareState(const AwareConfig& config)
  : m_config(config)
  , m_accepted(config.bins, 0.0)
  , m_rejected(config.bins, 0.0)
{
  FitMonotone();
}

void
DelayAwareState::OnForwarded(const ::ndn::Name& name, Time now, int hopLimit)
{
  m_forwarded[name] = {now, hopLimit};
}

void
DelayAwareState::OnUpstreamData(const ::ndn::Name& name, Time now, int answerHopLimit)
{
  auto it = m_forwarded.find(name);
  if (it == m_forwarded.end()) {
    return;
  }
  double rttMs = (now - it->second.first).GetSeconds() * 1000.0;
  Sample(rttMs);

  // HopLimit drops by one per node, so the difference is how far up the answer came from
  int hops = it->second.second - answerHopLimit;
  if (it->second.second >= 0 && answerHopLimit >= 0 && hops > 0) {
    double perHopMs = rttMs / hops;
    if (m_minHopMs < 0 || perHopMs < m_minHopMs) {
      m_minHopMs = perHopMs;
    }
  }
  m_forwarded.erase(it);
}

void
DelayAwareState::OnUpstreamExpired(const ::ndn::Name& name, Time now)
{
  auto it = m_forwarded.find(name);
  if (it == m_forwarded.end()) {
    return;
  }
  // a lost Interest or Data cost the client the whole lifetime: that belongs in D_up too
  Sample((now - it->second.first).GetSeconds() * 1000.0);
  m_forwarded.erase(it);
}

void
DelayAwareState::OnRequestSeen(uint64_t request, bool rejectedMe, Time now)
{
  for (auto it = m_answered.begin(); it != m_answered.end();) {
    if (now - it->second.second >= m_config.window) {
      Observe(it->second.first, true);
      it = m_answered.erase(it);
    }
    else {
      ++it;
    }
  }

  auto it = m_answered.find(request);
  if (it == m_answered.end()) {
    return;
  }
  // a retry that does not name this cache means the answer was lost, not rejected: no evidence
  if (rejectedMe) {
    Observe(it->second.first, false);
  }
  m_answered.erase(it);
}

void
DelayAwareState::OnAnswered(uint64_t request, double similarity, Time now)
{
  if (similarity >= 1 - 1e-9) {
    return; // an exact hit satisfies every user; nothing to learn
  }
  m_answered[request] = {Bin(similarity), now};
}

bool
DelayAwareState::ShouldAnswer(double similarity, uint32_t hops, Time delta, bool explore) const
{
  if (similarity >= 1 - 1e-9) {
    return true;
  }
  uint32_t bin = Bin(similarity);
  return (explore && Uncertain(bin, hops, delta)) || Worth(Accept(bin), hops, delta);
}

double
DelayAwareState::Threshold(uint32_t hops, Time delta) const
{
  for (uint32_t bin = 0; bin < m_config.bins; ++bin) {
    if (Worth(Accept(bin), hops, delta)) {
      return static_cast<double>(bin) / m_config.bins;
    }
  }
  return 1.0;
}

double
DelayAwareState::Accept(uint32_t bin) const
{
  return m_fit[bin];
}

void
DelayAwareState::FitMonotone()
{
  // pool adjacent violators over the Laplace-smoothed per-bin rates (an unseen bin is 1/2 with
  // weight 2), weighted by observations: the least-squares non-decreasing fit
  struct Block
  {
    double sum;
    double weight;
    uint32_t bins;
  };
  std::vector<Block> blocks;
  for (uint32_t bin = 0; bin < m_config.bins; ++bin) {
    double weight = m_accepted[bin] + m_rejected[bin] + 2;
    blocks.push_back({m_accepted[bin] + 1, weight, 1});
    while (blocks.size() > 1 && blocks[blocks.size() - 2].sum / blocks[blocks.size() - 2].weight >
                                  blocks.back().sum / blocks.back().weight) {
      Block last = blocks.back();
      blocks.pop_back();
      blocks.back().sum += last.sum;
      blocks.back().weight += last.weight;
      blocks.back().bins += last.bins;
    }
  }
  m_fit.clear();
  m_fitWeight.clear();
  for (const Block& block : blocks) {
    m_fit.insert(m_fit.end(), block.bins, block.sum / block.weight);
    m_fitWeight.insert(m_fitWeight.end(), block.bins, block.weight);
  }
}

uint32_t
DelayAwareState::Bin(double similarity) const
{
  return std::min(m_config.bins - 1, static_cast<uint32_t>(similarity * m_config.bins));
}

void
DelayAwareState::Observe(uint32_t bin, bool accepted)
{
  m_accepted[bin] = m_accepted[bin] * m_config.decay + (accepted ? 1 : 0);
  m_rejected[bin] = m_rejected[bin] * m_config.decay + (accepted ? 0 : 1);
  FitMonotone();
}

void
DelayAwareState::Sample(double rttMs)
{
  m_srttMs = m_srttMs < 0 ? rttMs : m_srttMs + m_config.gain * (rttMs - m_srttMs);
}

bool
DelayAwareState::Worth(double accept, uint32_t hops, Time delta) const
{
  if (m_srttMs < 0) {
    return false; // nothing measured upstream yet: forward and learn
  }
  double rttHMs = hops * std::max(m_minHopMs, 0.0);
  double penaltyMs = delta.GetSeconds() * 1000.0 + rttHMs + m_srttMs;
  return (1 - accept) * penaltyMs < m_srttMs;
}

bool
DelayAwareState::Uncertain(uint32_t bin, uint32_t hops, Time delta) const
{
  double f = Accept(bin);
  double margin = m_config.confidence * std::sqrt(f * (1 - f) / m_fitWeight[bin]);
  return Worth(std::min(1.0, f + margin), hops, delta) !=
         Worth(std::max(0.0, f - margin), hops, delta);
}

} // namespace simcache
} // namespace ns3
