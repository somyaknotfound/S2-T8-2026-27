/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Client of the base paper: requests an item, accepts the answer only if its similarity reaches
// the threshold, otherwise waits delta and re-requests. One request at a time.
// Under the threshold and exclusion policies each attempt also carries s_min and the caches
// that served rejected answers so far.

#ifndef SIMCACHE_SIM_CONSUMER_HPP
#define SIMCACHE_SIM_CONSUMER_HPP

#include "ns3/ndnSIM/apps/ndn-app.hpp"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"

#include <vector>

namespace ns3 {
namespace simcache {

class SimConsumer : public ndn::App
{
public:
  static TypeId
  GetTypeId();

  SimConsumer();

  void
  StartRequest(uint32_t requestId, uint32_t item, double threshold);

  void
  OnData(std::shared_ptr<const ::ndn::Data> data) override;

  void
  OnNack(std::shared_ptr<const ::ndn::lp::Nack> nack) override;

private:
  void
  SendAttempt();

  void
  OnTimeout();

  void
  RetryOrGiveUp(uint32_t servedNode, double similarity, Time wait);

  void
  Finish(uint32_t servedNode, double similarity, bool censored);

private:
  Ptr<UniformRandomVariable> m_rand;
  bool m_busy = false;
  uint32_t m_requestId = 0;
  uint32_t m_item = 0;
  double m_threshold = 0; // this request's user: minimum similarity accepted
  uint32_t m_attempt = 0;
  uint32_t m_timeouts = 0;
  uint32_t m_interestHops = 0;
  std::vector<uint32_t> m_excluded;
  Time m_firstSend;
  EventId m_timeoutEvent;
};

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_SIM_CONSUMER_HPP
