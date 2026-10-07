/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "gilbert-elliott-error-model.hpp"

#include "ns3/double.h"

namespace ns3 {
namespace simcache {

NS_OBJECT_ENSURE_REGISTERED(GilbertElliottErrorModel);

TypeId
GilbertElliottErrorModel::GetTypeId()
{
  static TypeId tid = TypeId("ns3::simcache::GilbertElliottErrorModel")
    .SetParent<ErrorModel>()
    .SetGroupName("Network")
    .AddConstructor<GilbertElliottErrorModel>()
    .AddAttribute("PGoodToBad", "Per-packet probability of moving from the good to the bad state",
                  DoubleValue(0), MakeDoubleAccessor(&GilbertElliottErrorModel::m_pGoodToBad),
                  MakeDoubleChecker<double>(0, 1))
    .AddAttribute("PBadToGood", "Per-packet probability of moving from the bad to the good state",
                  DoubleValue(1), MakeDoubleAccessor(&GilbertElliottErrorModel::m_pBadToGood),
                  MakeDoubleChecker<double>(0, 1))
    .AddAttribute("LossGood", "Loss probability in the good state", DoubleValue(0),
                  MakeDoubleAccessor(&GilbertElliottErrorModel::m_lossGood),
                  MakeDoubleChecker<double>(0, 1))
    .AddAttribute("LossBad", "Loss probability in the bad state", DoubleValue(1),
                  MakeDoubleAccessor(&GilbertElliottErrorModel::m_lossBad),
                  MakeDoubleChecker<double>(0, 1));
  return tid;
}

GilbertElliottErrorModel::GilbertElliottErrorModel()
  : m_rand(CreateObject<UniformRandomVariable>())
{
}

bool
GilbertElliottErrorModel::DoCorrupt(Ptr<Packet> /*packet*/)
{
  bool lost = m_rand->GetValue() < (m_bad ? m_lossBad : m_lossGood);
  if (m_rand->GetValue() < (m_bad ? m_pBadToGood : m_pGoodToBad)) {
    m_bad = !m_bad;
  }
  return lost;
}

void
GilbertElliottErrorModel::DoReset()
{
  m_bad = false;
}

} // namespace simcache
} // namespace ns3
