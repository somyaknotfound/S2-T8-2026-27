/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Wire format shared by the strategy, producer and consumer.
// Interest name: /sim/<item>/<clientNode>/<requestId>/<attempt>[/<paramsDigest>]
// Interest ApplicationParameters (threshold and exclusion policies only): "smin=<s>;excl=<n>,<n>"
// Data content : "<servedItem> <similarity> <servingNode>" under the Interest's own name,
// because NFD only accepts Data whose name matches the pending Interest (see design_notes Q2).

#ifndef SIMCACHE_SIM_COMMON_HPP
#define SIMCACHE_SIM_COMMON_HPP

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include <ndn-cxx/encoding/buffer.hpp>
#include <ndn-cxx/encoding/encoding-buffer.hpp>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ns3 {
namespace simcache {

struct Answer
{
  uint32_t item = 0;
  double similarity = 0;
  uint32_t node = 0;
};

struct RequestParams
{
  double sMin = 0;
  std::vector<uint32_t> excluded; // caches that served a rejected answer to this request
};

// cache and consumer must agree exactly; the tolerance absorbs the similarity's decimal round trip
inline bool
Acceptable(double similarity, double sMin)
{
  return similarity >= sMin - 1e-9;
}

inline bool
IsExcluded(const RequestParams& params, uint32_t nodeId)
{
  return std::find(params.excluded.begin(), params.excluded.end(), nodeId) !=
         params.excluded.end();
}

inline std::string
EncodeParams(const RequestParams& params)
{
  std::ostringstream os;
  os << "smin=" << std::setprecision(17) << params.sMin << ";excl=";
  for (size_t i = 0; i < params.excluded.size(); ++i) {
    os << (i > 0 ? "," : "") << params.excluded[i];
  }
  return os.str();
}

inline RequestParams
ParseParams(const std::string& text)
{
  RequestParams params;
  std::istringstream fields(text);
  std::string field;
  while (std::getline(fields, field, ';')) {
    auto eq = field.find('=');
    std::string key = field.substr(0, eq);
    std::string value = eq == std::string::npos ? "" : field.substr(eq + 1);
    if (key == "smin") {
      params.sMin = std::stod(value);
    }
    else if (key == "excl") {
      std::istringstream ids(value);
      std::string id;
      while (std::getline(ids, id, ',')) {
        params.excluded.push_back(static_cast<uint32_t>(std::stoul(id)));
      }
    }
  }
  return params;
}

inline RequestParams
ParseParams(const ::ndn::Interest& interest)
{
  if (!interest.hasApplicationParameters()) {
    return {};
  }
  auto block = interest.getApplicationParameters();
  return ParseParams(std::string(reinterpret_cast<const char*>(block.value()), block.value_size()));
}

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
