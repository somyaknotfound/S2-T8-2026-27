/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Wire format shared by the strategy, producer and consumer.
// Interest name: /sim/<item>/<clientNode>/<requestId>/<attempt>
// Data content : "<servedItem> <similarity> <servingNode>" under the Interest's own name,
// because NFD only accepts Data whose name matches the pending Interest (see design_notes Q2).

#ifndef SIMCACHE_SIM_COMMON_HPP
#define SIMCACHE_SIM_COMMON_HPP

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include <ndn-cxx/encoding/buffer.hpp>
#include <ndn-cxx/encoding/encoding-buffer.hpp>

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace ns3 {
namespace simcache {

struct Answer
{
  uint32_t item = 0;
  double similarity = 0;
  uint32_t node = 0;
};

inline bool
IsSimName(const ::ndn::Name& name)
{
  return name.size() >= 5 && name.at(0) == ::ndn::name::Component("sim");
}

inline ::ndn::Name
MakeSimName(uint32_t item, uint32_t clientNode, uint32_t requestId, uint32_t attempt)
{
  return ::ndn::Name("/sim")
    .appendNumber(item)
    .appendNumber(clientNode)
    .appendNumber(requestId)
    .appendNumber(attempt);
}

inline std::shared_ptr<::ndn::Data>
MakeAnswerData(const ::ndn::Name& name, const Answer& answer)
{
  std::ostringstream os;
  os << answer.item << ' ' << std::setprecision(17) << answer.similarity << ' ' << answer.node;
  std::string payload = os.str();

  auto data = std::make_shared<::ndn::Data>(name);
  data->setContent(std::make_shared<::ndn::Buffer>(payload.begin(), payload.end()));

  // same fake signature as ndn::Producer: simulation only, no crypto cost
  data->setSignatureInfo(::ndn::SignatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255)));
  ::ndn::EncodingEstimator estimator;
  ::ndn::EncodingBuffer encoder(estimator.appendVarNumber(0), 0);
  encoder.appendVarNumber(0);
  data->setSignatureValue(encoder.getBuffer());

  data->wireEncode();
  return data;
}

inline Answer
ParseAnswer(const ::ndn::Data& data)
{
  const auto& content = data.getContent();
  std::istringstream is(std::string(reinterpret_cast<const char*>(content.value()),
                                    content.value_size()));
  Answer answer;
  is >> answer.item >> answer.similarity >> answer.node;
  return answer;
}

} // namespace simcache
} // namespace ns3

#endif // SIMCACHE_SIM_COMMON_HPP
