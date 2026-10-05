/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Base-paper topology: caches v1..vN in tandem, origin v(N+1), one client on each cache.
//
//   client1   client2        clientN
//      |         |              |
//     v1 ------ v2 -- ... ---- vN ------ origin
//
// Run: ./waf --run "tandem --alpha=0.5 --threshold=0.95 --placement=per-attempt --run=1"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

#include "sim-consumer.hpp"
#include "sim-controller.hpp"
#include "sim-producer.hpp"
#include "similarity-strategy.hpp"

namespace ns3 {

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

int
main(int argc, char* argv[])
{
  simcache::SimConfig config;
  std::string placement = "static";
  uint32_t caches = 5;
  double deltaMs = 10;
  double linkDelayMs = 1;
  std::string linkRate = "1Gbps";

  CommandLine cmd;
  cmd.AddValue("alpha", "probability that a cache answers with its best match", config.alpha);
  cmd.AddValue("threshold", "minimum similarity the client accepts", config.threshold);
  cmd.AddValue("placement", "per-attempt | per-request | static", placement);
  cmd.AddValue("catalog", "number of items C", config.catalog);
  cmd.AddValue("cacheSize", "items per cache B", config.cacheSize);
  cmd.AddValue("caches", "number of caches in tandem", caches);
  cmd.AddValue("requests", "requests to simulate", config.requests);
  cmd.AddValue("maxAttempts", "attempts per request before it is logged as censored",
               config.maxAttempts);
  cmd.AddValue("deltaMs", "wait before re-requesting after a rejected answer", deltaMs);
  cmd.AddValue("linkDelayMs", "one-way propagation delay per link", linkDelayMs);
  cmd.AddValue("linkRate", "link data rate", linkRate);
  cmd.AddValue("run", "RNG run number", config.run);
  cmd.AddValue("out", "CSV output file", config.outFile);
  cmd.Parse(argc, argv);

  config.placement = ParsePlacement(placement);
  config.delta = MilliSeconds(deltaMs);
  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(config.run);

  NodeContainer nodes;
  nodes.Create(caches + 1);
  Ptr<Node> origin = nodes.Get(caches);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(linkRate));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(linkDelayMs)));
  p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("1000p"));
  for (uint32_t i = 0; i < caches; ++i) {
    p2p.Install(nodes.Get(i), nodes.Get(i + 1));
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

  controller.Start(Seconds(1));
  // NFD keeps periodic timers alive, so the controller stops the run when requests are done;
  // this is only a safety net
  Simulator::Stop(Seconds(1e7));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
