/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Answer-or-forward decision at a cache node. On a /sim Interest the node either answers with
// its most similar cached item (sendData under the Interest's name) or forwards like best-route.
// It also reports what it forwards, the Data that comes back and the Interests that expire, which
// the delay-aware policy turns into its upstream delay estimate.

#ifndef SIMCACHE_SIMILARITY_STRATEGY_HPP
#define SIMCACHE_SIMILARITY_STRATEGY_HPP

#include "ns3/ndnSIM/NFD/daemon/fw/best-route-strategy.hpp"

namespace nfd {
namespace fw {

class SimilarityStrategy : public BestRouteStrategy
{
public:
  explicit
  SimilarityStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

  void
  afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  // ndnSIM's NFD sends every incoming Data through satisfyInterest(), which calls this hook;
  // afterReceiveData() is never invoked in this fork (forwarder.cpp:341-347)
  void
  beforeSatisfyInterest(const Data& data, const FaceEndpoint& ingress,
                        const shared_ptr<pit::Entry>& pitEntry) override;

private:
  signal::ScopedConnection m_expiryConnection;
};

} // namespace fw
} // namespace nfd

#endif // SIMCACHE_SIMILARITY_STRATEGY_HPP
