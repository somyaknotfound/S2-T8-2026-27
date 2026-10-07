/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Two-state Markov burst-loss model. The chain advances once per packet; a packet is lost with
// the loss probability of the state it finds. With LossGood = 0 and LossBad = 1 the mean loss is
// pGB / (pGB + pBG) and the mean burst length is 1 / pBG packets.

#ifndef SIMCACHE_GILBERT_ELLIOTT_ERROR_MODEL_HPP
#define SIMCACHE_GILBERT_ELLIOTT_ERROR_MODEL_HPP

#include "ns3/error-model.h"
#include "ns3/random-variable-stream.h"

namespace ns3 {
namespace simcache {

class GilbertElliottErrorModel : public ErrorModel
{
public:
  static TypeId
  GetTypeId();

  GilbertElliottErrorModel();

private:
  bool
  DoCorrupt(Ptr<Packet> packet) override;

  void
  DoReset() override;

private:
  double m_pGoodToBad = 0;
  double m_pBadToGood = 1;
  double m_lossGood = 0;
  double m_lossBad = 1;
  bool m_bad = false;
  Ptr<UniformRandomVariable> m_rand;
};

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_GILBERT_ELLIOTT_ERROR_MODEL_HPP
