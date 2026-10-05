/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Origin server: always returns the exact requested item (similarity 1).

#ifndef SIMCACHE_SIM_PRODUCER_HPP
#define SIMCACHE_SIM_PRODUCER_HPP

#include "ns3/ndnSIM/apps/ndn-producer.hpp"

namespace ns3 {
namespace simcache {

class SimProducer : public ndn::Producer
{
public:
  static TypeId
  GetTypeId();

  void
  OnInterest(std::shared_ptr<const ::ndn::Interest> interest) override;
};

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_SIM_PRODUCER_HPP
