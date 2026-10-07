/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "similarity-strategy.hpp"
#include "sim-common.hpp"
#include "sim-controller.hpp"

#include "ns3/ndnSIM/NFD/daemon/fw/forwarder.hpp"

#include "ns3/log.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE("simcache.SimilarityStrategy");

namespace nfd {
namespace fw {

SimilarityStrategy::SimilarityStrategy(Forwarder& forwarder, const Name& name)
  // BestRouteStrategy rejects any instance name but its own, so hand it that and rename after
  : BestRouteStrategy(forwarder, BestRouteStrategy::getStrategyName())
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));

  // an Interest that dies unsatisfied (lost, or queued past its lifetime) still cost its client
  m_expiryConnection = forwarder.beforeExpirePendingInterest.connect([] (const pit::Entry& entry) {
    if (ns3::simcache::IsSimName(entry.getName())) {
      ns3::simcache::SimController::Get().OnUpstreamExpired(ns3::Simulator::GetContext(),
                                                            entry.getName());
    }
  });
}

const Name&
SimilarityStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/similarity").appendVersion(1);
  return strategyName;
}

void
SimilarityStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
  auto& controller = ns3::simcache::SimController::Get();
  // NFD itself finds the current node this way (NFD/daemon/rib/service.cpp)
  uint32_t nodeId = ns3::Simulator::GetContext();
  const Name& name = interest.getName();

  if (!controller.IsCache(nodeId) || !ns3::simcache::IsSimName(name)) {
    BestRouteStrategy::afterReceiveInterest(interest, ingress, pitEntry);
    return;
  }

  auto item = static_cast<uint32_t>(name.at(1).toNumber());
  auto best = controller.BestMatch(nodeId, item);

  auto params = ns3::simcache::ParseParams(interest);
  params.client = static_cast<uint32_t>(name.at(2).toNumber());
  params.request = static_cast<uint32_t>(name.at(3).toNumber());
  int hopLimit = ns3::simcache::HopLimitOf(interest);
  // NFD has already decremented it here, once per node from the client's own on
  params.hops = hopLimit >= 0 ? ns3::simcache::kHopLimit - 1 - hopLimit : 0;

  if (!controller.ShouldAnswer(nodeId, best.second, params)) {
    NS_LOG_DEBUG("node " << nodeId << " forwards " << name << " hopLimit " << hopLimit);
    controller.OnForwarded(nodeId, name, hopLimit);
    BestRouteStrategy::afterReceiveInterest(interest, ingress, pitEntry);
    return;
  }

  auto data = ns3::simcache::MakeAnswerData(name, {best.first, best.second, nodeId, hopLimit});
  this->sendData(*data, ingress.face, pitEntry);
}

void
SimilarityStrategy::beforeSatisfyInterest(const Data& data, const FaceEndpoint& ingress,
                                          const shared_ptr<pit::Entry>& pitEntry)
{
  if (ns3::simcache::IsSimName(data.getName())) {
    NS_LOG_DEBUG("node " << ns3::Simulator::GetContext() << " gets Data " << data.getName());
    ns3::simcache::SimController::Get().OnUpstreamData(ns3::Simulator::GetContext(),
                                                       data.getName(),
                                                       ns3::simcache::ParseAnswer(data).hopLimit);
  }
  BestRouteStrategy::beforeSatisfyInterest(data, ingress, pitEntry);
}

} // namespace fw
} // namespace nfd
