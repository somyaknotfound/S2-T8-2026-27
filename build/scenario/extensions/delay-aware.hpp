/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// What one delay-aware cache learns, and its answer-or-forward rule (stages/S4, "Implementation
// design"). A cache answers with its best match s' iff the expected cost of a rejection is below
// the cost of going upstream:
//   (1 - F(s')) * (delta + RTT_h + D_up) < D_up
// F(s')  : share of this cache's answers in the bin of s' that users accepted (learned from
//          retries), fitted non-decreasing in s' (whoever accepts s' accepts anything closer)
// D_up   : smoothed RTT of the Interests this cache forwarded, counting the ones that expired
// RTT_h  : h hops back to the client times the smallest per-hop RTT seen (h from HopLimit)

#ifndef SIMCACHE_DELAY_AWARE_HPP
#define SIMCACHE_DELAY_AWARE_HPP

#include "ns3/nstime.h"

#include <ndn-cxx/name.hpp>

#include <map>
#include <utility>
#include <vector>

namespace ns3 {
namespace simcache {

struct AwareConfig
{
  uint32_t bins = 20;
  // explore: answer anyway with probability epsilon, but only while the decision is uncertain,
  // i.e. the rule gives different answers at F +- confidence * (standard error of F). Exploring
  // stops once a bin's decision is settled and resumes if the load moves it near the boundary.
  double epsilon = 1.0;
  double confidence = 2.0;
  Time window = Seconds(1); // an answer not rejected within this long counts as accepted
  double decay = 1.0;       // weight kept by older answers per new one; < 1 tracks a drifting
                            // user population (every experiment here has a fixed one)
  double gain = 0.125;      // SRTT gain, as in TCP (Jacobson/Karels)
};

class DelayAwareState
{
public:
  explicit DelayAwareState(const AwareConfig& config);

  // upstream measurements, from the strategy
  void
  OnForwarded(const ::ndn::Name& name, Time now, int hopLimit);

  void
  OnUpstreamData(const ::ndn::Name& name, Time now, int answerHopLimit);

  void
  OnUpstreamExpired(const ::ndn::Name& name, Time now);

  // feedback on this cache's own answers; a request id identifies (client, request)
  void
  OnRequestSeen(uint64_t request, bool rejectedMe, Time now);

  void
  OnAnswered(uint64_t request, double similarity, Time now);

  bool
  ShouldAnswer(double similarity, uint32_t hops, Time delta, bool explore) const;

  // smallest s' this cache would answer for a client h hops away (1 if only exact hits)
  double
  Threshold(uint32_t hops, Time delta) const;

  double
  UpstreamMs() const
  {
    return m_srttMs;
  }

  double
  HopMs() const
  {
    return m_minHopMs;
  }

  double
  Accept(uint32_t bin) const;

  uint32_t
  Bins() const
  {
    return m_config.bins;
  }

private:
  uint32_t
  Bin(double similarity) const;

  void
  Observe(uint32_t bin, bool accepted);

  void
  FitMonotone();

  void
  Sample(double rttMs);

  bool
  Worth(double accept, uint32_t hops, Time delta) const;

  bool
  Uncertain(uint32_t bin, uint32_t hops, Time delta) const;

private:
  AwareConfig m_config;
  std::map<::ndn::Name, std::pair<Time, int>> m_forwarded; // name -> (sent, HopLimit seen here)
  std::map<uint64_t, std::pair<uint32_t, Time>> m_answered; // request -> (bin, answered at)
  std::vector<double> m_accepted;
  std::vector<double> m_rejected;
  std::vector<double> m_fit;       // isotonic F per bin
  std::vector<double> m_fitWeight; // observations (+ prior) pooled into each bin's fitted value
  double m_srttMs = -1;   // no sample yet
  double m_minHopMs = -1; // no sample yet
};

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_DELAY_AWARE_HPP
