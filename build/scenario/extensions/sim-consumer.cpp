/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "sim-consumer.hpp"
#include "sim-common.hpp"
#include "sim-controller.hpp"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

#include <limits>

NS_LOG_COMPONENT_DEFINE("simcache.SimConsumer");

namespace ns3 {
namespace simcache {

NS_OBJECT_ENSURE_REGISTERED(SimConsumer);

TypeId
SimConsumer::GetTypeId()
{
  static TypeId tid = TypeId("ns3::simcache::SimConsumer")
    .SetGroupName("Ndn")
    .SetParent<ndn::App>()
    .AddConstructor<SimConsumer>();
  return tid;
}

SimConsumer::SimConsumer()
  : m_rand(CreateObject<UniformRandomVariable>())
{
}

void
SimConsumer::StartRequest(uint32_t requestId, uint32_t item)
{
  NS_ABORT_MSG_IF(m_busy, "client " << GetNode()->GetId() << " already has a request");
  m_busy = true;
  m_requestId = requestId;
  m_item = item;
  m_attempt = 0;
  m_timeouts = 0;
  m_firstSend = Simulator::Now();
  SendAttempt();
}

void
SimConsumer::SendAttempt()
{
  if (!m_active || !m_busy) {
    return;
  }
  ++m_attempt;

  const auto& config = SimController::Get().GetConfig();
  auto interest = std::make_shared<::ndn::Interest>(
    MakeSimName(m_item, GetNode()->GetId(), m_requestId, m_attempt));
  interest->setNonce(m_rand->GetInteger(0, std::numeric_limits<uint32_t>::max()));
  interest->setCanBePrefix(false);
  interest->setInterestLifetime(
    ::ndn::time::milliseconds(config.interestLifetime.GetMilliSeconds()));

  m_timeoutEvent = Simulator::Schedule(config.interestLifetime, &SimConsumer::OnTimeout, this);

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
}

void
SimConsumer::OnData(std::shared_ptr<const ::ndn::Data> data)
{
  if (!m_active || !m_busy) {
    return;
  }
  ndn::App::OnData(data);

  const auto& name = data->getName();
  if (!IsSimName(name) || name.at(3).toNumber() != m_requestId ||
      name.at(4).toNumber() != m_attempt) {
    return; // answer to an earlier attempt that already timed out
  }
  Simulator::Cancel(m_timeoutEvent);

  Answer answer = ParseAnswer(*data);
  const auto& config = SimController::Get().GetConfig();
  // tolerance guards against the similarity's decimal round trip through the Data payload
  if (answer.similarity >= config.threshold - 1e-9) {
    Finish(answer.node, answer.similarity, false);
    return;
  }
  RetryOrGiveUp(answer.node, answer.similarity, config.delta);
}

void
SimConsumer::OnNack(std::shared_ptr<const ::ndn::lp::Nack> nack)
{
  if (!m_active || !m_busy) {
    return;
  }
  ndn::App::OnNack(nack);
  Simulator::Cancel(m_timeoutEvent);
  ++m_timeouts;
  RetryOrGiveUp(GetNode()->GetId(), 0, Seconds(0));
}

void
SimConsumer::OnTimeout()
{
  ++m_timeouts;
  RetryOrGiveUp(GetNode()->GetId(), 0, Seconds(0));
}

void
SimConsumer::RetryOrGiveUp(uint32_t servedNode, double similarity, Time wait)
{
  if (m_attempt >= SimController::Get().GetConfig().maxAttempts) {
    Finish(servedNode, similarity, true);
    return;
  }
  Simulator::Schedule(wait, &SimConsumer::SendAttempt, this);
}

void
SimConsumer::Finish(uint32_t servedNode, double similarity, bool censored)
{
  m_busy = false;

  RequestRecord record;
  record.requestId = m_requestId;
  record.clientNode = GetNode()->GetId();
  record.item = m_item;
  record.attempts = m_attempt;
  record.timeouts = m_timeouts;
  record.delayMs = (Simulator::Now() - m_firstSend).GetSeconds() * 1000.0;
  record.servedNode = servedNode;
  record.similarity = similarity;
  record.censored = censored;
  SimController::Get().OnRequestDone(record);
}

} // namespace simcache
} // namespace ns3
