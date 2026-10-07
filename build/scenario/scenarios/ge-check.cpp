/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
// Check of GilbertElliottErrorModel against its closed forms: with LossGood = 0, LossBad = 1,
// mean loss = pGB / (pGB + pBG) and mean burst = 1 / pBG packets.
// Run: ge-check --pgb=0.0158 --pbg=0.3 --packets=2000000

#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "gilbert-elliott-error-model.hpp"

#include <iostream>

int
main(int argc, char* argv[])
{
  using namespace ns3;
  double pgb = 0.0158;
  double pbg = 0.3;
  uint32_t packets = 2000000;
  CommandLine cmd;
  cmd.AddValue("pgb", "good->bad probability", pgb);
  cmd.AddValue("pbg", "bad->good probability", pbg);
  cmd.AddValue("packets", "packets to draw", packets);
  cmd.Parse(argc, argv);

  auto model = CreateObject<simcache::GilbertElliottErrorModel>();
  model->SetAttribute("PGoodToBad", DoubleValue(pgb));
  model->SetAttribute("PBadToGood", DoubleValue(pbg));
  Ptr<Packet> packet = Create<Packet>(100);

  uint64_t lost = 0, bursts = 0;
  bool previous = false;
  for (uint32_t i = 0; i < packets; ++i) {
    bool now = model->IsCorrupt(packet);
    lost += now;
    bursts += now && !previous;
    previous = now;
  }
  std::cout << "loss " << double(lost) / packets << " (expected " << pgb / (pgb + pbg)
            << "), mean burst " << double(lost) / bursts << " (expected " << 1 / pbg << ")\n";
  return 0;
}
