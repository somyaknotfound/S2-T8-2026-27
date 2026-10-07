/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Base-paper topology: caches v1..vN in tandem, origin v(N+1), one client on each cache.
//
//   client1   client2        clientN
//      |         |              |
//     v1 ------ v2 -- ... ---- vN ------ origin
//
// Run: ./waf --run "tandem --policy=threshold --alpha=0.5 --threshold=0.95 --run=1"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

#include "gilbert-elliott-error-model.hpp"
#include "sim-consumer.hpp"
#include "sim-controller.hpp"
#include "sim-producer.hpp"
#include "similarity-strategy.hpp"

#include <fstream>
#include <sstream>

namespace ns3 {

// Whole-run network counters for --runOut, counted from the first request onwards.
struct RunStats
{
  Time from;
  uint64_t crossSent = 0;
  uint64_t crossReceived = 0;
  double crossDelaySumMs = 0;
  uint64_t queueDrops = 0;
  uint64_t errorDrops = 0;
  uint64_t received = 0; // packets that survived the error model
  std::vector<uint64_t> txBytesDown; // per link, toward the clients
  std::vector<uint64_t> txBytesUp;   // per link, toward the origin
  std::vector<double> linkBps;
};

static RunStats g_stats;

static void
CountPacket(uint64_t* counter, Ptr<const Packet>)
{
  if (Simulator::Now() >= g_stats.from) {
    ++*counter;
  }
}

static void
CountBytes(uint64_t* counter, Ptr<const Packet> packet)
{
  if (Simulator::Now() >= g_stats.from) {
    *counter += packet->GetSize();
  }
}

static void
OnCrossInterest(std::shared_ptr<const ndn::Interest>, Ptr<ndn::App>, std::shared_ptr<ndn::Face>)
{
  if (Simulator::Now() >= g_stats.from) {
    ++g_stats.crossSent;
  }
}

static void
OnCrossData(Ptr<ndn::App>, uint32_t, Time delay, uint32_t, int32_t)
{
  if (Simulator::Now() >= g_stats.from) {
    ++g_stats.crossReceived;
    g_stats.crossDelaySumMs += delay.GetSeconds() * 1000.0;
  }
}

static void
WriteRunStats(const std::string& path, const simcache::SimConfig& config,
              const std::string& policy, const std::string& placement,
              const std::string& thresholds, uint32_t crossPayload)
{
  double seconds = (Simulator::Now() - g_stats.from).GetSeconds();
  double downMax = 0, downSum = 0, upMax = 0;
  for (size_t i = 0; i < g_stats.linkBps.size(); ++i) {
    double capacityBytes = g_stats.linkBps[i] * seconds / 8.0;
    downMax = std::max(downMax, g_stats.txBytesDown[i] / capacityBytes);
    downSum += g_stats.txBytesDown[i] / capacityBytes;
    upMax = std::max(upMax, g_stats.txBytesUp[i] / capacityBytes);
  }

  std::ofstream os(path);
  NS_ABORT_MSG_UNLESS(os.is_open(), "cannot open " << path);
  os << "run,policy,alpha,threshold,thresholds,placement,link_rate,bottleneck_rate,"
        "queue_packets,cross_rate,"
        "cross_payload,loss_pgb,loss_pbg,requests,interest_lifetime_ms,measured_s,cross_sent,"
        "cross_received,cross_delay_ms,queue_drops,error_drops,received_packets,util_down_max,"
        "util_down_mean,util_up_max\n";
  os << config.run << ',' << policy << ',' << config.alpha << ',' << config.threshold << ",\""
     << thresholds << "\"," << placement << ',' << config.linkRate << ','
     << config.bottleneckRate << ',' << config.queuePackets
     << ',' << config.crossRate << ',' << crossPayload << ',' << config.lossPgb << ','
     << config.lossPbg << ',' << config.requests << ','
     << config.interestLifetime.GetMilliSeconds() << ',' << seconds << ',' << g_stats.crossSent
     << ',' << g_stats.crossReceived << ',';
  if (g_stats.crossReceived > 0) {
    os << g_stats.crossDelaySumMs / g_stats.crossReceived;
  }
  os << ',' << g_stats.queueDrops << ',' << g_stats.errorDrops << ',' << g_stats.received << ','
     << downMax << ','
     << downSum / g_stats.txBytesDown.size() << ',' << upMax << '\n';
}

static std::vector<double>
ParseList(const std::string& text)
{
  std::vector<double> values;
  std::istringstream is(text);
  std::string value;
  while (std::getline(is, value, ',')) {
    values.push_back(std::stod(value));
  }
  return values;
}

static simcache::Placement
ParsePlacement(const std::string& value)
{
  if (value == "per-attempt") {
    return simcache::Placement::PerAttempt;
  }
  if (value == "per-request") {
    return simcache::Placement::PerRequest;
  }
  NS_ABORT_MSG_UNLESS(value == "static", "placement must be per-attempt, per-request or static");
  return simcache::Placement::Static;
}

static simcache::Policy
ParsePolicy(const std::string& value)
{
  if (value == "blind") {
    return simcache::Policy::Blind;
  }
  if (value == "threshold") {
    return simcache::Policy::Threshold;
  }
  if (value == "delay-aware") {
    return simcache::Policy::DelayAware;
  }
  NS_ABORT_MSG_UNLESS(value == "exclusion",
                      "policy must be blind, threshold, exclusion or delay-aware");
  return simcache::Policy::Exclusion;
}

int
main(int argc, char* argv[])
{
  simcache::SimConfig config;
  std::string policy = "blind";
  std::string placement = "static";
  uint32_t caches = 5;
  double deltaMs = 10;
  double linkDelayMs = 1;
  double interestLifetimeMs = 2000;
  std::string thresholds;
  uint32_t crossPayload = 1024;
  std::string runOut;
  double awareWindowMs = 1000;

  CommandLine cmd;
  cmd.AddValue("policy", "blind | threshold | exclusion | delay-aware", policy);
  cmd.AddValue("alpha", "probability that a cache answers with its best match", config.alpha);
  cmd.AddValue("threshold", "minimum similarity the client accepts", config.threshold);
  cmd.AddValue("thresholds", "mixed users: each request draws its s from this comma list",
               thresholds);
  cmd.AddValue("placement", "per-attempt | per-request | static", placement);
  cmd.AddValue("catalog", "number of items C", config.catalog);
  cmd.AddValue("cacheSize", "items per cache B", config.cacheSize);
  cmd.AddValue("caches", "number of caches in tandem", caches);
  cmd.AddValue("requests", "requests to simulate", config.requests);
  cmd.AddValue("maxAttempts", "attempts per request before it is logged as censored",
               config.maxAttempts);
  cmd.AddValue("deltaMs", "wait before re-requesting after a rejected answer", deltaMs);
  cmd.AddValue("linkDelayMs", "one-way propagation delay per link", linkDelayMs);
  cmd.AddValue("linkRate", "data rate of every tandem link", config.linkRate);
  cmd.AddValue("bottleneckRate", "data rate of the last cache-to-origin link (backhaul)",
               config.bottleneckRate);
  cmd.AddValue("queuePackets", "DropTail transmit queue per link direction, in packets",
               config.queuePackets);
  cmd.AddValue("interestLifetimeMs", "lifetime of the clients' Interests", interestLifetimeMs);
  cmd.AddValue("crossRate", "cross traffic from v1 to the origin, Interests/s (Poisson)",
               config.crossRate);
  cmd.AddValue("crossPayload", "cross-traffic Data payload, bytes", crossPayload);
  cmd.AddValue("lossPgb", "Gilbert-Elliott good->bad probability per packet, every link",
               config.lossPgb);
  cmd.AddValue("lossPbg", "Gilbert-Elliott bad->good probability per packet", config.lossPbg);
  cmd.AddValue("run", "RNG run number", config.run);
  cmd.AddValue("out", "CSV output file", config.outFile);
  cmd.AddValue("runOut", "optional one-row CSV of network counters for the run", runOut);
  cmd.AddValue("nodeOut", "optional CSV of each cache's learned state (delay-aware)",
               config.nodeOutFile);
  cmd.AddValue("nodeLogEvery", "requests between nodeOut rows", config.nodeLogEvery);
  cmd.AddValue("awareEpsilon", "delay-aware: probability of answering anyway while a bin's "
               "decision is uncertain", config.aware.epsilon);
  cmd.AddValue("awareConfidence", "delay-aware: standard errors of F that make a decision "
               "certain", config.aware.confidence);
  cmd.AddValue("awareWindowMs", "delay-aware: an answer not rejected within this is accepted",
               awareWindowMs);
  cmd.AddValue("awareDecay", "delay-aware: weight kept by older answers per new one in a bin",
               config.aware.decay);
  cmd.AddValue("awareBins", "delay-aware: similarity bins for the learned acceptance",
               config.aware.bins);
  cmd.Parse(argc, argv);
  config.aware.window = MilliSeconds(awareWindowMs);

  config.policy = ParsePolicy(policy);
  config.placement = ParsePlacement(placement);
  config.delta = MilliSeconds(deltaMs);
  config.interestLifetime = MilliSeconds(interestLifetimeMs);
  config.thresholds = ParseList(thresholds);
  NS_ABORT_MSG_IF(config.lossPgb > 0 && config.lossPbg <= 0, "lossPgb needs lossPbg > 0");
  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(config.run);

  NodeContainer nodes;
  nodes.Create(caches + 1);
  Ptr<Node> origin = nodes.Get(caches);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(config.linkRate));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(linkDelayMs)));
  p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize",
               StringValue(std::to_string(config.queuePackets) + "p"));
  std::vector<NetDeviceContainer> links;
  for (uint32_t i = 0; i < caches; ++i) {
    if (i == caches - 1 && !config.bottleneckRate.empty()) {
      p2p.SetDeviceAttribute("DataRate", StringValue(config.bottleneckRate));
    }
    links.push_back(p2p.Install(nodes.Get(i), nodes.Get(i + 1)));
    g_stats.linkBps.push_back(
      DataRate(i == caches - 1 && !config.bottleneckRate.empty() ? config.bottleneckRate :
                                                                   config.linkRate).GetBitRate());
  }

  // names are unique per request, so NFD's exact-match Content Store never hits; keep it minimal
  ndn::StackHelper ndnHelper;
  ndnHelper.setCsSize(1);
  ndnHelper.InstallAll();

  NodeContainer cacheNodes;
  for (uint32_t i = 0; i < caches; ++i) {
    cacheNodes.Add(nodes.Get(i));
  }
  ndn::StrategyChoiceHelper::Install<nfd::fw::SimilarityStrategy>(cacheNodes, "/sim");

  ndn::GlobalRoutingHelper routing;
  routing.InstallAll();
  routing.AddOrigins("/sim", origin);
  if (config.crossRate > 0) {
    routing.AddOrigins("/cross", origin);
  }

  ndn::AppHelper producerHelper("ns3::simcache::SimProducer");
  producerHelper.SetPrefix("/sim");
  producerHelper.Install(origin);

  ndn::GlobalRoutingHelper::CalculateRoutes();

  auto& controller = simcache::SimController::Get();
  controller.Configure(config);
  for (uint32_t i = 0; i <= caches; ++i) {
    controller.AddNode(nodes.Get(i)->GetId(), i, i < caches);
  }

  ndn::AppHelper consumerHelper("ns3::simcache::SimConsumer");
  for (uint32_t i = 0; i < caches; ++i) {
    ApplicationContainer apps = consumerHelper.Install(nodes.Get(i));
    controller.AddClient(DynamicCast<simcache::SimConsumer>(apps.Get(0)), nodes.Get(i)->GetId());
  }

  // Everything below creates random streams only when enabled, and after the controller and
  // clients, so their streams (and hence default runs) stay exactly as in earlier stages.
  if (config.crossRate > 0) {
    ndn::AppHelper crossProducer("ns3::ndn::Producer");
    crossProducer.SetPrefix("/cross");
    crossProducer.SetAttribute("PayloadSize", UintegerValue(crossPayload));
    crossProducer.Install(origin);

    ndn::AppHelper crossConsumer("ns3::ndn::ConsumerCbr");
    crossConsumer.SetPrefix("/cross");
    crossConsumer.SetAttribute("Frequency", DoubleValue(config.crossRate));
    crossConsumer.SetAttribute("Randomize", StringValue("exponential"));
    Ptr<Application> app = crossConsumer.Install(nodes.Get(0)).Get(0);
    app->TraceConnectWithoutContext("TransmittedInterests", MakeCallback(&OnCrossInterest));
    app->TraceConnectWithoutContext("FirstInterestDataDelay", MakeCallback(&OnCrossData));
  }

  if (config.lossPgb > 0) {
    for (auto& link : links) {
      for (uint32_t d = 0; d < link.GetN(); ++d) {
        auto model = CreateObject<simcache::GilbertElliottErrorModel>();
        model->SetAttribute("PGoodToBad", DoubleValue(config.lossPgb));
        model->SetAttribute("PBadToGood", DoubleValue(config.lossPbg));
        link.Get(d)->SetAttribute("ReceiveErrorModel", PointerValue(model));
      }
    }
  }

  // link i joins node i (device 0, sends up) and node i+1 (device 1, sends down)
  g_stats.from = Seconds(1);
  g_stats.txBytesDown.assign(caches, 0);
  g_stats.txBytesUp.assign(caches, 0);
  for (uint32_t i = 0; i < caches; ++i) {
    links[i].Get(0)->TraceConnectWithoutContext("PhyTxEnd",
                                                MakeBoundCallback(&CountBytes, &g_stats.txBytesUp[i]));
    links[i].Get(1)->TraceConnectWithoutContext("PhyTxEnd",
                                                MakeBoundCallback(&CountBytes,
                                                                  &g_stats.txBytesDown[i]));
    for (uint32_t d = 0; d < 2; ++d) {
      links[i].Get(d)->TraceConnectWithoutContext("MacTxDrop",
                                                  MakeBoundCallback(&CountPacket,
                                                                    &g_stats.queueDrops));
      links[i].Get(d)->TraceConnectWithoutContext("PhyRxDrop",
                                                  MakeBoundCallback(&CountPacket,
                                                                    &g_stats.errorDrops));
      links[i].Get(d)->TraceConnectWithoutContext("PhyRxEnd",
                                                  MakeBoundCallback(&CountPacket,
                                                                    &g_stats.received));
    }
  }

  controller.Start(g_stats.from);
  // NFD keeps periodic timers alive, so the controller stops the run when requests are done;
  // this is only a safety net
  Simulator::Stop(Seconds(1e7));
  Simulator::Run();
  if (!runOut.empty()) {
    WriteRunStats(runOut, config, policy, placement, thresholds, crossPayload);
  }
  Simulator::Destroy();
  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
