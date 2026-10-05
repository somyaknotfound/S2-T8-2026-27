/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "similarity-strategy.hpp"
#include "sim-common.hpp"
#include "sim-controller.hpp"

#include "ns3/simulator.h"

namespace nfd {
namespace fw {

SimilarityStrategy::SimilarityStrategy(Forwarder& forwarder, const Name& name)
  // BestRouteStrategy rejects any instance name but its own, so hand it that and rename after
  : BestRouteStrategy(forwarder, BestRouteStrategy::getStrategyName())
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
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
  if (!controller.ShouldAnswer(nodeId, best.second)) {
    BestRouteStrategy::afterReceiveInterest(interest, ingress, pitEntry);
    return;
  }

  auto data = ns3::simcache::MakeAnswerData(name, {best.first, best.second, nodeId});
  this->sendData(*data, ingress.face, pitEntry);
}

} // namespace fw
} // namespace nfd
