/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "sim-producer.hpp"
#include "sim-common.hpp"

#include "ns3/node.h"

namespace ns3 {
namespace simcache {

NS_OBJECT_ENSURE_REGISTERED(SimProducer);

TypeId
SimProducer::GetTypeId()
{
  static TypeId tid = TypeId("ns3::simcache::SimProducer")
    .SetGroupName("Ndn")
    .SetParent<ndn::Producer>()
    .AddConstructor<SimProducer>();
  return tid;
}

void
SimProducer::OnInterest(std::shared_ptr<const ::ndn::Interest> interest)
{
  ndn::App::OnInterest(interest); // tracing; skips Producer's own reply

  if (!m_active || !IsSimName(interest->getName())) {
    return;
  }

  auto item = static_cast<uint32_t>(interest->getName().at(1).toNumber());
  auto data = MakeAnswerData(interest->getName(), {item, 1.0, GetNode()->GetId()});

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);
}

} // namespace simcache
} // namespace ns3
